/*
 * MSE_OS_API.c
 *
 *  Created on: 12 abr. 2020
 *      Author: gonza
 */


#include "../../NF_MSE_OS_V2/inc/MSE_OS_API.h"
#include "board.h"		//-- Solo para debug, comentar si no es necesario
#include "sapi.h"		//-- Solo para debug, comentar si no es necesario

/*************************************************************************************************
	 *  @brief delay no preciso en base a ticks del sistema
     *
     *  @details
     *   Para utilizar un delay en el OS se vale del tick de sistema para contabilizar cuantos
     *   ticks debe una tarea estar bloqueada.
     *
	 *  @param		ticks	Cantidad de ticks de sistema que esta tarea debe estar bloqueada
	 *  @return     None.
	 *  @warning	No puede llamarse a delay desde un handler, produce un error de OS
***************************************************************************************************/
void os_Delay(uint32_t ticks)  {
	tarea* tarea_actual;

	/*
	 * Esta prohibido llamar esta funcion de API desde un handler, genera un error que
	 * detiene el OS
	 */


	if(os_getEstadoSistema() == OS_IRQ_RUN)  {
		os_setError(ERR_OS_DELAY_FROM_ISR,os_Delay);
	}

	if(ticks > 0)  {
		os_enter_critical();

		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		tarea_actual = os_getTareaActual();
		tarea_actual->ticks_bloqueada = ticks;
		tarea_actual->blocked_by_delay =true;
		//----------------------------------------------------------------------------------------------------

		os_exit_critical();

		//-- Si la tarea actual esta SUSPENDIDA
		//-- se ignora el delay
		if(tarea_actual->estado == TAREA_SUSPENDED){
			return;
		}
		/*
		 * El proximo bloque while tiene la finalidad de asegurarse que la tarea solo se desbloquee
		 * en el momento que termine la cuenta de ticks. Si por alguna razon la tarea se vuelve a
		 * ejecutar antes que termine el periodo de bloqueado, queda atrapada.
		 * La bandera delay activo es puesta en false por el SysTick. En teoria esto no deberia volver
		 * a ejecutarse dado que el scheduler no vuelve a darle CPU hasta que no pase a estado READY
		 *
		 */

		else while (tarea_actual->ticks_bloqueada > 0)  {
			tarea_actual->estado = TAREA_BLOCKED;
			os_setRegBlockedCnt(os_getRegBlockedCnt(tarea_actual->prioridad)+1,tarea_actual->prioridad);
			os_setRegBlocked((1<<(tarea_actual->id)) | os_getRegBlocked());
			os_CpuYield();
		}
	}
}
/*************************************************************************************************
	 *  @brief Delay para ser utilizado en tareas que requieran una ejecucion periòdica
     *
     *  @details
     *  Sabiendo el valor de ticks inicial de la tarea y el valor de ticks actual se puede determinar
     *  cuanto tiempo paso desde un suceso a otro. Por lo cual realizando la resta de este valor
     *  con respecto a la cantidad de ticks seteados en el delay se puede determinar el tiempo
     *  remanente que debe esperar dicha tarea.
     *
     *  En caso de que el tiempo efectivo sea menor a 0 la tarea no se bloquea y puede seguir
     *  ejecutandose normalmente.
     *
     *  Esta tarea utiliza una variable de ticks counter que puede hacer overflow cada 50 dìas
     *  por lo cual en esta situaciòn se detecta dicha situaciòn y en esta caso particular
     *  el delay será simplemente el valor ingresado como parametro ticks.
     *
	 *  @param1		ticks	Cantidad de ticks de sistema que esta tarea debe estar bloqueada
	 *  @param2		init_ticks Valor del contador de ticks que poseia al momento de iniciar la tarea.
	 *  @return     None.
	 *  @warning	No puede llamarse a delay desde un handler, produce un error de OS
***************************************************************************************************/

void os_DelayUntil(uint32_t ticks, uint32_t init_ticks){
	tarea* tarea_actual;

	//-- effective_ticks: Se utiliza como variable auxiliar priemro para obtener el tick count y luego para
	//-- obtener los ticks efectivos del bloqueo de la tarea.
	uint32_t effective_ticks;

	/*
	 * Esta prohibido llamar esta funcion de API desde un handler, genera un error que
	 * detiene el OS
	 */
	if(os_getEstadoSistema() == OS_IRQ_RUN)  {
		os_setError(ERR_OS_DELAY_FROM_ISR,os_Delay);
	}
	if(ticks > 0)  {
		os_enter_critical();
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		tarea_actual = os_getTareaActual();

		effective_ticks = os_getTickCounter();

		//-- Los ticks efectivos son igual a la resta de ticks con respecto al tiempo que sucedio
		effective_ticks = ticks - (effective_ticks - init_ticks);

		//-- Con este resultado pueden suceder dos situaciones particulares:

		if(effective_ticks > ticks){
			//-- La primer situación corresponde al overflow del contador del systick aproximadamente cada 50
			//-- días. En este caso se puede detectar ya que el resultado de effective_tick sera notoriamente
			//-- mayor a cualquier valor de ticks ingresado. Por lo cual en ese caso, al no saber cuanto tiempo
			//-- ha sucedido desde el inicio de la ejecuciòn de la tarea bloquea directamente la tarea con el
			//-- delay en ticks que vino como parámetro.
			effective_ticks = ticks;
		}

		if(effective_ticks >0){
			//-- La segunda situaciòn es que el còdigo a ejecutar haya demorado tanto como para igualar o superar
			//-- el valor de ticks del delay. Por lo cual no es necesario esperar mas, la tarea no es bloqueada.


			tarea_actual->ticks_bloqueada = effective_ticks;
			tarea_actual->blocked_by_delay =true;
		}
		else{
			tarea_actual->ticks_bloqueada = 0;
			tarea_actual->blocked_by_delay =false;
		}

		//----------------------------------------------------------------------------------------------------
		os_exit_critical();

		//-- Si la tarea actual esta SUSPENDIDA
		//-- se ignora el delay
		if(tarea_actual->estado == TAREA_SUSPENDED){
			return;
		}
		//-- El proximo bloque while tiene la finalidad de asegurarse que la tarea solo se desbloquee
		//-- en el momento que termine la cuenta de ticks.
		//-- Si el delay no fue necesario ticks_bloqueada sera cero por lo cual es ignorado dicho bloque de
		//-- de código.
		else while (tarea_actual->ticks_bloqueada > 0)  {
			tarea_actual->estado = TAREA_BLOCKED;
			os_setRegBlockedCnt(os_getRegBlockedCnt(tarea_actual->prioridad)+1,tarea_actual->prioridad);
			os_setRegBlocked((1<<(tarea_actual->id)) | os_getRegBlocked());
			os_CpuYield();
		}
	}
}
//-----------------------------Ejemplo de utilizaciòn del delay until--------------------------------------
/*
void ex_delay_until(void)  {
	uint32_t init_value;

	while(1)  {
		init_value = os_getTickCounter();

		//----------------------------------------------------
		//-----Zona de ejecuciòn de codigo periodico----------
		//----------------------------------------------------

		os_DelayUntil(200,init_value); //-- la porciòn de código se ejecutara cada 200ms
									   //-- siempre y cuando la tarea no quede restringida
									   //-- por otra tarea de mayor prioridad
		//
	}
}
*/




/*************************************************************************************************
	 *  @brief Inicializacion de un semaforo binario
     *
     *  @details
     *   Antes de utilizar cualquier semaforo binario en el sistema, debe inicializarse el mismo.
     *   Todos los semaforos se inicializan tomados
     *
	 *  @param		sem		Semaforo a inicializar
	 *  @return     None.
***************************************************************************************************/
void os_SemaforoInit(osSemaforo* sem)  {
	sem->tomado = true;
	sem->tarea_asociada = NULL;
}



/*************************************************************************************************
	 *  @brief Tomar un semaforo
     *
     *  @details
     *   Esta funcion es utilizada para tomar un semaforo cualquiera.
     *
     *   ticks_blocked es ignorado si se llama desde una ISR, en este caso solo se hace consulta
     *   si se puede tomar o no y retorna el valor resultante.
     *
     *   ticks_blocked es el tiempo que se mantiene bloqueado el semáforo
     *
     *   ticks_blocked es 0: 			se hace polling del semaforo.
     *   ticks_blocked es negativo: 	se bloquea indefinidamente hasta que la tarea
     *   								logra tomar el semaforo.
     *
	 *  @param		sem		Semaforo a tomar
	 *  @param2   	ticks_blocked  Tiempo que se mantiene bloqueado.
	 *  @return     true	Si se pudo tomar el semaforo.
	 *  			false	Si no se pudo tomar el semáforo.
***************************************************************************************************/
bool os_SemaforoTake(osSemaforo* sem, int32_t ticks_blocked)  {
	bool Salir = false;
	tarea* tarea_actual;

	if( (os_getEstadoSistema() == OS_IRQ_RUN) || (ticks_blocked == 0 ) ){
		//-- Si estamos en este bloque significa que la funcionalidad del semaforo es por polling.
		//-- Por lo cual se consulta el estado del semaforo, se toma si se puede y se retorna el
		//-- resultado obtenido.
		if(sem->tomado)  {
			return false;
		}
		else{
			sem->tomado = true;
			return true;
		}

	}
	else if(ticks_blocked > 0){
		if(sem->tomado)  {
			os_enter_critical();

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			tarea_actual = os_getTareaActual();
			if(tarea_actual->estado != TAREA_SUSPENDED){
				tarea_actual->estado = TAREA_BLOCKED;
				tarea_actual->blocked_by_delay =true;
				tarea_actual->ticks_bloqueada = ticks_blocked;
				sem->tarea_asociada = tarea_actual;
				os_setRegBlockedCnt(os_getRegBlockedCnt(sem->tarea_asociada->prioridad)+1,sem->tarea_asociada->prioridad);
				os_setRegBlocked((1<<tarea_actual->id) | os_getRegBlocked());
			}

			//---------------------------------------------------------------------------

			os_exit_critical();
			os_CpuYield();


			//-- Cuando la tarea vuelva a estado RUNNING puede ser por dos situaciones
			//-- o el contador llego a cero por lo cual no se pudo tomar el semaforo en
			//-- ese tiempo.
			//-- o se pudo tomar el semaforo satisfactoriamente dentro del tiempo
			//-- estipulado.
			if(tarea_actual->ticks_bloqueada == 0){
				return false;
			}
			else{
				return true;
			}
		}
		else{
			//-- El semaforo está libre y puede ser tomado.
			sem->tomado = true;
			return true;
		}

	}
	else{
		//-- Si ticks_blocked es menor a 0 indica que el semaforo debe ser esperado
		//-- indefinidamente hasta que pueda ser tomado.
		if(sem->tomado)  {
			os_enter_critical();

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			tarea_actual = os_getTareaActual();
			if(tarea_actual->estado != TAREA_SUSPENDED){
				tarea_actual->estado = TAREA_BLOCKED;
				tarea_actual->blocked_by_delay =false;
				sem->tarea_asociada = tarea_actual;
				os_setRegBlockedCnt(os_getRegBlockedCnt(sem->tarea_asociada->prioridad)+1,sem->tarea_asociada->prioridad);
				os_setRegBlocked((1<<tarea_actual->id) | os_getRegBlocked());
			}
			//---------------------------------------------------------------------------

			os_exit_critical();
			os_CpuYield();

			//-- Luego de desbloquearse la tarea el semaforo puede ser tomado
			//-- se vuelve a reverificar que se pueda tomar el semaforo para
			//-- evitar cualquier problema.
			//-- en este caso es la unica situaciòn donde puede retornar false
			//-- para un valor de ticks_blocked negativo y sin ser llamado de
			//-- una ISR.
			if(sem->tomado){
				return false;
			}
			else{
				sem->tomado = true;
				return true;
			}
		}
		else  {
			sem->tomado = true;
			return true;
		}

	}


}



/********************************************************************************
	 *  @brief Liberar un semaforo
     *
     *  @details
     *   Esta funcion es utilizada para liberar un semaforo cualquiera.
     *
	 *  @param		sem		Semaforo a liberar
	 *  @return     None.
 *******************************************************************************/
void os_SemaforoGive(osSemaforo* sem)  {

	/*
	 * Por seguridad, se deben hacer dos checkeos antes de hacer un give sobre
	 * el semaforo. En el caso de que se ambas condiciones sean verdaderas, se
	 * libera y se actualiza la tarea correspondiente a estado ready.
	 */

	if (sem->tomado == true &&	sem->tarea_asociada != NULL)  {
		if(sem->tarea_asociada->estado != TAREA_SUSPENDED){
			os_enter_critical();
			//-- No se permite la atención de otra interrupción mientras se manipulan
			//-- las variables del sistema operativo
			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			sem->tomado = false;
			sem->tarea_asociada->estado = TAREA_READY;
			os_setRegBlockedCnt(os_getRegBlockedCnt(sem->tarea_asociada->prioridad)-1,sem->tarea_asociada->prioridad);
			os_setRegBlocked( os_getRegBlocked() & (~(1<<sem->tarea_asociada->id)) );

			//---------------------------------------------------------------------------
			os_exit_critical();

			//-- Habilitamos el scheduler para que se atienda a la tarea que espera el evento.

			if (os_getEstadoSistema() == OS_IRQ_RUN)
				 os_setScheduleDesdeISR(true);
		}

	}
	else if(sem->tarea_asociada == NULL){
		os_setError(SEMAPH_HANDLE_NULL,os_SemaforoGive);
	}
}


/*************************************************************************************************
	 *  @brief Inicializacion de una cola
     *
     *  @details
     *   Antes de utilizar cualquier cola en el sistema, debe inicializarse la misma.
     *   Todas las colas se inicializan vacias y sin una tarea asociada. Aqui se determina
     *   cuantos elementos (espacios) tendra disponible la cola dado el tamaño de la cola
     *   en bytes definida por la constante QUEUE_HEAP_SIZE y el tamaño en bytes de cada
     *   elemento que se desea almacenar. Es una forma facil de determinar los limites
     *   de los indices head y tail en el momento de la operacion. Se puede volver a inicializar
     *   una cola para resetearla y cambiar el tipo de datos que contiene
     *
	 *  @param		cola		Cola a inicializar
	 *  @param		datasize	Tamaño de los elementos que seran almacenados en la cola.
	 *  						Debe ser pasado mediante la funcion sizeof()
	 *  @return     None.
	 *
	 *  @warning	Esta inicializacion fija el tamaño de cada elemento, por lo que no vuelve
	 *  			a consultarse en otras funciones, pasar datos con otros tamaños en funciones
	 *  			de escritura y lectura puede dar lugar a corrupcion de datos.
***************************************************************************************************/
void os_ColaInit(osCola* cola, uint16_t datasize)  {
	cola->indice_head = 0;
	cola->indice_tail = 0;
	cola->tarea_asociada = NULL;
	cola->size_elemento = datasize;
}


/*************************************************************************************************
	 *  @brief Escritura en una cola
     *
     *  @details
     *
     *
	 *  @param		cola		Cola donde escribir el dato
	 *  @param		dato		Puntero a void del dato a escribir
	 *  @return     None.
***************************************************************************************************/
void os_ColaWrite(osCola* cola, void* dato)  {
	uint16_t index_h;					//variable para legibilidad
	uint16_t elementos_total;		//variable para legibilidad
	tarea* tarea_actual;

	index_h = cola->indice_head * cola->size_elemento;
	elementos_total = QUEUE_HEAP_SIZE / cola->size_elemento;


	/*
	 * el primer bloque determina tres cosas, que gracias a la disposicion de los
	 * parentesis se dan en un orden especifico:
	 * 1) Se determina si head == tail, con lo que la cola estaria vacia
	 * 2) Sobre el resultado de 1) se determina si existe una tarea asociada
	 * 3) Si la cola esta vacia, y el puntero a la tarea asociada es valido, se
	 * 		verifica si la tarea asociada esta bloqueada
	 * Estas condiciones en ese orden determinan si se trato de leer de una cola vacia
	 * y la tarea que quizo leer se bloqueo porque la misma estaba vacia. Como
	 * seguramente en este punto se escribe un dato a la misma, esa tarea tiene que
	 * pasar a ready
	 */

	if(((cola->indice_head == cola->indice_tail) && cola->tarea_asociada != NULL) &&
		cola->tarea_asociada->estado == TAREA_BLOCKED)  {
		cola->tarea_asociada->estado = TAREA_READY;
		os_setRegBlocked(os_getRegBlocked() & (~(1<<cola->tarea_asociada->id)));
		os_setRegBlockedCnt(os_getRegBlockedCnt(cola->tarea_asociada->prioridad)-1,cola->tarea_asociada->prioridad);
		/*
		 * Si es llamada desde una interrupcion, se debe indicar que es necesario efectuar
		 * un scheduling, porque seguramente existe una tarea esperando este evento
		 */
		if (os_getEstadoSistema() == OS_IRQ_RUN)
			os_setScheduleDesdeISR(true);

	}



	 /*
	 * En el caso de que se quiera escribir una cola desde un ISR y este
	 * llena, la operacion es abortada (no se puede bloquear un handler)
	 */
	if (os_getEstadoSistema() == OS_IRQ_RUN &&
			(cola->indice_head + 1) % elementos_total == cola->indice_tail)  {

		os_setWarning(WARN_OS_QUEUE_FULL_ISR);
		return;		//operacion abortada
	}


	/*
	 * El siguiente bloque while determina que hasta que la cola no tenga lugar
	 * disponible, no se avance. Si no tiene lugar se bloquea la tarea actual
	 * que es la que esta tratando de escribir y luego se hace un yield
	 */
	while((cola->indice_head + 1) % elementos_total == cola->indice_tail)  {

		os_enter_critical();

		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		tarea_actual = os_getTareaActual();
		if(tarea_actual->estado == TAREA_BLOCKED){
			tarea_actual->estado = TAREA_BLOCKED;
			tarea_actual->blocked_by_delay =false;
			cola->tarea_asociada = tarea_actual;
			os_setRegBlocked(os_getRegBlocked() | (1<<cola->tarea_asociada->id));
			os_setRegBlockedCnt(os_getRegBlockedCnt(cola->tarea_asociada->prioridad)+1,cola->tarea_asociada->prioridad);
		}

		//---------------------------------------------------------------------------

		os_exit_critical();
		os_CpuYield();
	}

	/*
	 * Si la cola tiene lugar, se escribe mediante la funcion memcpy que copia un
	 * bloque completo de memoria iniciando desde la direccion apuntada por el
	 * primer elemento. Como data es un vector del tipo uint8_t, la aritmetica
	 * de punteros es byte a byte (consecutivos) y se logra el efecto deseado
	 * Esto permite guardar datos definidos por el usuario, como ser estructuras
	 * de datos completas. Luego se actualiza el undice head y se limpia la tarea
	 * asociada, dado que ese puntero ya no tiene utilidad
	 */

	memcpy(cola->data+index_h,dato,cola->size_elemento);
	cola->indice_head = (cola->indice_head + 1) % elementos_total;
	cola->tarea_asociada = NULL;
}

void os_ColaRead(osCola* cola, void* dato)  {
	uint16_t elementos_total;		//variable para legibilidad
	uint16_t index_t;					//variable para legibilidad
	tarea* tarea_actual;

	if(cola->tarea_asociada->estado != TAREA_SUSPENDED){
		index_t = cola->indice_tail * cola->size_elemento;
		elementos_total = QUEUE_HEAP_SIZE / cola->size_elemento;


		/*
		 * el primer bloque determina tres cosas, que gracias a la disposicion de los
		 * parentesis se dan en un orden especifico:
		 * 1) Se determina si la cola esta llena (head+1)%CANT_ELEMENTOS == tail
		 * 2) Sobre el resultado de 1) se determina si existe una tarea asociada
		 * 3) Si la cola esta llena, y el puntero a la tarea asociada es valido, se
		 * 		verifica si la tarea asociada esta bloqueada
		 * Estas condiciones en ese orden determinan si se trato de escribir en una cola
		 * llena y la tarea que quizo escribir se bloqueo porque la misma estaba llena. Como
		 * seguramente en este punto se lee un dato de la misma, esa tarea tiene que
		 * pasar a ready
		 */

		if((( (cola->indice_head + 1) % elementos_total == cola->indice_tail) &&
				cola->tarea_asociada != NULL) &&
				cola->tarea_asociada->estado == TAREA_BLOCKED)  {

			cola->tarea_asociada->estado = TAREA_READY;
			os_setRegBlockedCnt(os_getRegBlockedCnt(cola->tarea_asociada->prioridad)-1,cola->tarea_asociada->prioridad);
			os_setRegBlocked(os_getRegBlocked() & (~(1<<cola->tarea_asociada->id)));
			/*
			 * Si es llamada desde una interrupcion, se debe indicar que es necesario efectuar
			 * un scheduling, porque seguramente existe una tarea esperando este evento
			 */
			if (os_getEstadoSistema() == OS_IRQ_RUN)
				os_setScheduleDesdeISR(true);
		}


		/*
		 * En el caso de que se quiera leer una cola desde un ISR y este
		 * vacia, la operacion es abortada (no se puede bloquear un handler)
		 */
		if (os_getEstadoSistema() == OS_IRQ_RUN && cola->indice_head == cola->indice_tail)  {
			os_setWarning(WARN_OS_QUEUE_EMPTY_ISR);
			return;		//operacion abortada
		}

		/*
		 * El siguiente bloque while determina que hasta que la cola no tenga un dato
		 * disponible, no se avance. Si no hay un dato que leer, se bloquea la tarea
		 * actual que es la que esta tratando de leer un dato y luego se hace un yield
		 */

		while(cola->indice_head == cola->indice_tail)  {
			os_enter_critical();

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			tarea_actual = os_getTareaActual();
			if(tarea_actual->estado != TAREA_SUSPENDED){
				tarea_actual->estado = TAREA_BLOCKED;
				tarea_actual->blocked_by_delay =false;
				cola->tarea_asociada = tarea_actual;
				os_setRegBlockedCnt(os_getRegBlockedCnt(tarea_actual->prioridad)+1,tarea_actual->prioridad);
				os_setRegBlocked( os_getRegBlocked() | (1<<cola->tarea_asociada->id));
			}
			//---------------------------------------------------------------------------

			os_exit_critical();
			os_CpuYield();
		}

		/*
		 * Si la cola tiene datos, se lee mediante la funcion memcpy que copia un
		 * bloque completo de memoria iniciando desde la direccion apuntada por el
		 * primer elemento. Como data es un vector del tipo uint8_t, la aritmetica
		 * de punteros es byte a byte (consecutivos) y se logra el efecto deseado
		 * Esto permite guardar datos definidos por el usuario, como ser estructuras
		 * de datos completas. Luego se actualiza el undice head y se limpia la tarea
		 * asociada, dado que ese puntero ya no tiene utilidad
		 */

		memcpy(dato,cola->data+index_t,cola->size_elemento);
		cola->indice_tail = (cola->indice_tail + 1) % elementos_total;
		cola->tarea_asociada = NULL;
	}


}

