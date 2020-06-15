/*
 * MSE_OS_Core.h
 *
 *  Created on: 26 mar. 2020
 *      Author: gonza
 */

#ifndef ISO_I_2020_MSE_OS_INC_MSE_OS_CORE_H_
#define ISO_I_2020_MSE_OS_INC_MSE_OS_CORE_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "board.h"


/************************************************************************************
 * 			Tamaño del stack predefinido para cada tarea expresado en bytes
 ***********************************************************************************/

#define STACK_SIZE 256

//----------------------------------------------------------------------------------



/************************************************************************************
 * 	Posiciones dentro del stack de los registros que lo conforman
 ***********************************************************************************/

#define XPSR			1
#define PC_REG			2
#define LR				3
#define R12				4
#define R3				5
#define R2				6
#define R1				7
#define R0				8
#define LR_PREV_VALUE	9
#define R4				10
#define R5				11
#define R6				12
#define R7				13
#define R8				14
#define R9				15
#define R10 			16
#define R11 			17

//----------------------------------------------------------------------------------


/************************************************************************************
 * 			Valores necesarios para registros del stack frame inicial
 ***********************************************************************************/

#define INIT_XPSR 	1 << 24				//xPSR.T = 1
#define EXEC_RETURN	0xFFFFFFF9			//retornar a modo thread con MSP, FPU no utilizada

//----------------------------------------------------------------------------------


/************************************************************************************
 * 						Definiciones varias
 ***********************************************************************************/
#define STACK_FRAME_SIZE			8
#define FULL_STACKING_SIZE 			17	//16 core registers + valor previo de LR

#define TASK_NAME_SIZE				10	//tamaño array correspondiente al nombre
#define MAX_TASK_COUNT				8	//cantidad maxima de tareas para este OS

#define MAX_PRIORITY		0			//maxima prioridad que puede tener una tarea
#define MIN_PRIORITY		3			//minima prioridad que puede tener una tarea

#define PRIORITY_COUNT		(MIN_PRIORITY-MAX_PRIORITY)+1	//cantidad de prioridades asignables

#define QUEUE_HEAP_SIZE		64			//cantidad de bytes reservados por cada cola definida



/*==================[definicion codigos de error y warning de OS]=================================*/
#define ERR_OS_CANT_TAREAS		-1
#define ERR_OS_SCHEDULING		-2
#define ERR_OS_DELAY_FROM_ISR	-3
#define ERR_OS_VAL_PRIO_TASK    -4
#define IDLE_NOT_READY_OR_RUN   -5
#define TASK_PRIO_CNT_OVERFL	-6
#define SEMAPH_HANDLE_NULL		-7

#define WARN_OS_QUEUE_FULL_ISR	-100
#define WARN_OS_QUEUE_EMPTY_ISR	-101



/*==================[definicion de datos para el OS]=================================*/

/********************************************************************************
 * Definicion de los estados posibles para las tareas
 *******************************************************************************/

enum _estadoTarea  {
	TAREA_READY,
	TAREA_RUNNING,
	TAREA_BLOCKED,
	TAREA_SUSPENDED
};

typedef enum _estadoTarea estadoTarea;


/********************************************************************************
 * Definicion de los estados posibles de nuestro OS
 *******************************************************************************/

enum _estadoOS  {
	OS_FROM_RESET,				//inicio luego de un reset
	OS_NORMAL_RUN,				//estado del sistema corriendo una tarea
	OS_SCHEDULING,				//el OS esta efectuando un scheduling
	OS_IRQ_RUN,					//El OS esta corriendo un Handler
	OS_ERROR					//Se produjo un error en el sistema.

};

typedef enum _estadoOS estadoOS;


/********************************************************************************
 * Definicion de la estructura para cada tarea
 *******************************************************************************/
struct _tarea  {
	uint32_t stack[STACK_SIZE/4];
	uint32_t stack_pointer;
	void *entry_point;
	uint8_t id;
	estadoTarea estado;
	uint8_t prioridad;
	uint32_t ticks_bloqueada;					//-- Ticks que la tarea debe estar bloqueada.
	bool blocked_by_delay;						//-- Determina si el systick debe decrementar los ticks o se trata
												//-- de un bloqueo independientemente del tiempo.
};

typedef struct _tarea tarea;



/********************************************************************************
 * Definicion de la estructura de control para el sistema operativo
 *******************************************************************************/
struct _osControl  {
	tarea *listaTareas[MAX_TASK_COUNT];			//array de punteros a tareas
	int32_t error;								//variable que contiene el ultimo error generado
	uint8_t cantidad_Tareas;					//cantidad de tareas definidas por el usuario
	uint8_t cantTareas_prioridad[PRIORITY_COUNT];	//cada posicion contiene cuantas tareas tienen la misma prioridad

	tarea *arr_priority[4][8];						//-- Arreglo de punteros a tarea de:
													//-- filas = cantidad de prioridades
													//-- Columnas = cantidad de tareas.
	//-- Se incorpora un registro contador de 16bits (reg_priorityCnt) que posee internamente la cantidad de tareas
	//-- que se encuentran en cada prioridad. Si el contador es 0 no hay ninguna tarea en ese nivel de
	//-- prioridad. Los valores de 1 a 8 indican la cantidad de tareas.
	//-- La estructura de este registro es la siguiente:
	//--      -----------------------------------------------------------------------------
	//--      | cont_prioridad_3 | cont_prioridad_2 | cont_prioridad_1 | cont_prioridad_0 | <--- Reg contador
	//--      -------4bits--------------4bits---------------4bits-------------4bits--------      (uint16_t)

	uint16_t reg_priorityCnt;

	//-- Se incorpora un registro indice de 16bits (index_tasks) que posee internamente la posición de
	//-- la próxima tarea a ejecutar dentro del Round Robin.
	//-- En OS_Init se inicializa en con el valor mas alto. En el scheduler se compara el indice de cada prioridad
	//-- con el respectivo contador en caso de ser igual o mayor, se pone el indice en 0 para que comience nuevamente
	//-- por el primer elemento del vector que corresponde a cada prioridad.
	//-- La estructura de este registro es la siguiente:
	//--      ---------------------------------------------------------------------------------
	//--      | index_prioridad_3 | index_prioridad_2 | index_prioridad_1 | index_prioridad_0 | <--- index_tasks
	//--      -------4bits---------------4bits----------------4bits--------------4bits---------      (uint16_t)

	uint16_t index_tasks;

	//-- Para mejorar la performance en el scheduler se incorpora un registo de 16bits que posee 4 contadores
	//-- que determinan cuántas tareas hay bloqueadas en cada nivel de prioridad.
	//-- Al comparar el valor de cada contador con su respectivo nivel de prioridad se puede determinar:
	//-- si cont_bloq_prioridad_3 < cont_prioridad_3 --> Hay tareas dentro de ese nivel que deben ser atendidas
	//-- si cont_bloq_prioridad_3 = cont_prioridad_3 --> Todas las tareas de ese nivel se encuentran actualmente
	//-- bloqueadas.
	//-- La estructura de este registro es la siguiente:
	//--  ----------------------------------------- blocked_cnt (uint16_t) --------------------------------
	//--  -------------------------------------------------------------------------------------------------
	//--  | cont_bloq_prioridad_3 | cont_bloq_prioridad_2 | cont_bloq_prioridad_1 | cont_bloq_prioridad_0 |
	//--  -------4bits----------------------4bits-------------------4bits-----------------4bits------------

	uint16_t blocked_cnt;

	//-- Registro lógico qué determina que tareas se encuentran bloqueadas con respecto al orden de tareas de
	//-- control_OS.listaTareas[].
	uint8_t reg_blocked;

	//-- Incrementa el tiempo en ticks cada vez que se utiliza el Systick
	//-- Nota: esta variable puede hacer overflow por lo cual cada 49.7 dias
	//-- aproximadamente el valor obtenido comenzará de cero nuevamente.
	//-- (para un systick que interrumpe cada 1ms)
	uint32_t 	tick_counter;

	estadoOS estado_sistema;					//Informacion sobre el estado del OS
	bool cambioContextoNecesario;
	bool schedulingFromIRQ;						//esta bandera se utiliza para la atencion a interrupciones
	int16_t contador_critico;					//Contador de secciones criticas solicitadas


	tarea *tarea_actual;				//definicion de puntero para tarea actual
	tarea *tarea_siguiente;			//definicion de puntero para tarea siguiente
};
typedef struct _osControl osControl;


/*==================[definicion de prototipos]=================================*/

// ------------------------- INITS -------------------------------
uint8_t os_InitTarea(void *entryPoint, tarea *task, uint8_t prioridad);
void os_Init(void);

// ------------------------- GETS -------------------------------
int32_t os_getError(void);
tarea* os_getTareaActual(void);
estadoOS os_getEstadoSistema(void);
bool os_getScheduleDesdeISR(void);
estadoOS os_getRegBlocked(void);
uint8_t os_getRegBlockedCnt(uint8_t priority);
uint32_t os_getTickCounter(void);
// ------------------------- SETS -------------------------------
void os_setEstadoSistema(estadoOS estado);
void os_setScheduleDesdeISR(bool value);
void os_setError(int32_t err, void* caller);
void os_setRegBlocked(uint8_t reg_actualizado);
void os_setRegBlockedCnt(uint8_t cnt,uint8_t priority);

// ------------------------- SETS -------------------------------
void os_CpuYield(void);

// ----------------- ATOMIC OPERATIONS --------------------------
void os_enter_critical(void);
void os_exit_critical(void);

//----------------- TASK SUSPEND / RESUME ----------------------
void os_taskSuspend(uint8_t index);
void os_taskResume(uint8_t index);




#endif /* ISO_I_2020_MSE_OS_INC_MSE_OS_CORE_H_ */
