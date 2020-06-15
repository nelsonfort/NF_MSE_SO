/*==================[inclusions]=============================================*/

#include "../../NF_MSE_OS_V2/inc/main.h"

#include <stdlib.h>

#include "../../NF_MSE_OS_V2/inc/MSE_OS_API.h"
#include "../../NF_MSE_OS_V2/inc/MSE_OS_Core.h"
#include "board.h"
#include "sapi.h"

// Problema de compilaciòn
// Agregar estas linker flags para que compile correctamente -specs=nano.specs -specs=nosys.specs

/*==================[macros and definitions]=================================*/

#define MILISEC		1000

#define PRIORIDAD_0		0
#define PRIORIDAD_1		1
#define PRIORIDAD_2		2
#define PRIORIDAD_3		3


#define TEC1_PORT_NUM   0
#define TEC1_BIT_VAL    4

#define TEC2_PORT_NUM   0
#define TEC2_BIT_VAL    8

/*==================[Global data declaration]==============================*/

tarea g_sEncenderLed, g_sApagarLed;	//prioridad 0
tarea g_sBlink, g_sBlinkUntil;	//prioridad 2
tarea g_sUart;	//prioridad 3

osCola colaUart;

osSemaforo semTecla1_descendente, semTecla1_ascendente;

typedef struct _mydata my_data;

/*==================[internal functions declaration]=========================*/

void tecla1_down_ISR(void);
void tecla1_up_ISR(void);

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/** @brief hardware initialization function
 *	@return none
 */
static void initHardware(void)  {
	Board_Init();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock / MILISEC);		//systick 1ms

	/*
	 * Seteamos la interrupcion 0 para el flanco descendente en la tecla 1
	 */
	Chip_SCU_GPIOIntPinSel( 0, TEC1_PORT_NUM, TEC1_BIT_VAL );
	Chip_PININT_ClearIntStatus( LPC_GPIO_PIN_INT, PININTCH( 0 ) ); // INT0 flanco descendente
	Chip_PININT_SetPinModeEdge( LPC_GPIO_PIN_INT, PININTCH( 0 ) );
	Chip_PININT_EnableIntLow( LPC_GPIO_PIN_INT, PININTCH( 0 ) );

	/*
	 * Seteamos la interrupcion 1 para el flanco ascendente en la tecla 1
	 */
	Chip_SCU_GPIOIntPinSel( 1, TEC1_PORT_NUM, TEC1_BIT_VAL );
	Chip_PININT_ClearIntStatus( LPC_GPIO_PIN_INT, PININTCH( 1 ) ); // INT1 flanc
	Chip_PININT_SetPinModeEdge( LPC_GPIO_PIN_INT, PININTCH( 1 ) );
	Chip_PININT_EnableIntHigh( LPC_GPIO_PIN_INT, PININTCH( 1 ) );

	/* Inicializar UART_USB a 115200 baudios */
	uartConfig( UART_USB, 115200 );
}


/*==================[Definicion de tareas para el OS]==========================*/
void encenderLed(void)  {
	char msg[25];
	uint8_t i;

	strcpy(msg,"Se presiono la tecla 1\n\r");

	while (1) {

		if(os_SemaforoTake(&semTecla1_descendente,2000)){
			gpioWrite(LED1,true);
			i = 0;
			while(msg[i] != NULL)  {
				os_ColaWrite(&colaUart,(msg + i));
				i++;
			}
		}
		else{
			gpioWrite(LED2,true);
			//os_taskResume(3);
		}

	}
}

void apagarLed(void)  {
	char msg[25];
	uint8_t i;

	strcpy(msg,"Se solto la tecla 1\n\r");

	while (1) {

		if(os_SemaforoTake(&semTecla1_ascendente,3000)){
			gpioWrite(LED1,false);
			i = 0;
			while(msg[i] != NULL)  {
				os_ColaWrite(&colaUart,(msg + i));
				i++;
			}
		}
		else{
			gpioWrite(LED2,false);
			//os_taskSuspend(3);
		}


	}
}


void uart(void)  {
	char aux;

	while(1)  {
		os_ColaRead(&colaUart,&aux);
		uartWriteByte(UART_USB,aux);
	}
}

void blink(void)  {


	while(1)  {
		gpioToggle(LEDB);
		//for(uint32_t i=0; i<=700000; i++);
		os_Delay(100);
	}
}
void blink_until(void)  {
	uint32_t init_value;

	while(1)  {
		init_value = os_getTickCounter();

		//----------------------------------------------------
		//-----Zona de ejecuciòn de codigo periodico----------
		gpioToggle(LED3);
		for(uint32_t i=0; i<=700000; i++);
		//----------------------------------------------------

		os_DelayUntil(400,init_value); //-- la porciòn de código se ejecutara cada 200ms
									   //-- siempre y cuando la tarea no quede restringida
									   //-- por otra tarea de mayor prioridad.
	}
}


/*============================================================================*/

int main(void)  {

	initHardware();

	os_InitTarea(encenderLed, &g_sEncenderLed,PRIORIDAD_0);
	os_InitTarea(apagarLed, &g_sApagarLed,PRIORIDAD_0);
	os_InitTarea(uart, &g_sUart,PRIORIDAD_3);
	os_InitTarea(blink, &g_sBlink,PRIORIDAD_2);
	os_InitTarea(blink_until, &g_sBlinkUntil,PRIORIDAD_2);
	//os_InitTarea(blink, &g_sBlink,4);  //-- Prueba de error de prioridad
	os_ColaInit(&colaUart,sizeof(char));
	os_SemaforoInit(&semTecla1_ascendente);
	os_SemaforoInit(&semTecla1_descendente);

	os_InstalarIRQ(PIN_INT0_IRQn,tecla1_down_ISR);
	os_InstalarIRQ(PIN_INT1_IRQn,tecla1_up_ISR);
	os_Init();

	while (1) {
	}
}


void tecla1_down_ISR(void) {
	os_SemaforoGive(&semTecla1_descendente);
	Chip_PININT_ClearIntStatus( LPC_GPIO_PIN_INT, PININTCH( 0 ) );
}

void tecla1_up_ISR(void)  {
	os_SemaforoGive(&semTecla1_ascendente);
	Chip_PININT_ClearIntStatus( LPC_GPIO_PIN_INT, PININTCH( 1 ) );
}

/** @} doxygen end group definition */

/*==================[end of file]============================================*/
