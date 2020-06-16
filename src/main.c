/*==================[inclusions]=============================================*/

#include "../../NF_MSE_OS_V2/inc/main.h"


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


//--------------------- Tareas asociadas al sistema ----------------------------

//-- Prioridad 0 --
tarea task_estadoTec1; 	//-- Atiende los sucesos para ambos flancos
tarea task_estadoTec2; 	//-- Atiende los sucesos para ambos flancos

//-- Prioridad 1 --
tarea task_processor; 	//-- Valida los 4 valores medidos.
						//-- Realiza el cálculo de tiempos.
						//-- Determina tiempo leido y led asociado
						//-- Envía resultado a la tarea que enciende
						//-- el led.
						//-- Envía los valores medidos a la tarea que
						//-- se encarga de transmitir los mensajes por
						//-- uart

tarea task_armarMsg;    //-- Para mantener el sistema modular se
						//-- crea una tarea que en base a las mediciones
						//-- arma strings con los resultados obtenidos.

//-- Prioridad 2 --
tarea task_uartSend;    //-- Recibe el texto asociado a cada una de las
						//-- Mediciones asociadas
tarea task_encenderLed;	//-- Recibe led y tiempo de encendido y ejecuta
						//-- la operaciòn.

//-- Prioridad 3 no se utiliza para ver que el scheduler ignora
//.. las tareas de dicha prioridad por no haber ninguna asociada.

//----------------------- Colas -----------------------------------------
//-- Para el transpaso de mensajes. Se evita la utilizaciòn de variables
//-- globales.
osCola colaTecData;
osCola colaEncLedData;
osCola colaDataMsg;
osCola colaUart;

//------------------- Sincronizacion -----------------------------------
osSemaforo SemBinTec1RiseEdge;
osSemaforo SemBinTec1FallEdge;
osSemaforo SemBinTec2RiseEdge;
osSemaforo SemBinTec2FallEdge;

osSemaforo SemBinMsgTransmit;



/*==================[internal functions declaration]=========================*/
void TEC1_IRQHandler(void);
void TEC2_IRQHandler(void);

static void initHardware(void);
void configInterrupts(void);

//-- Prioridad 0 --
void t_estadoTec1(void);
void t_estadoTec2(void);
//-- Prioridad 1 --
void t_processor(void);
void t_armarMsg(void);

//-- Prioridad 2 --
void t_uartSend(void);
void t_encenderLed(void);

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/** @brief hardware initialization function
 *	@return none
 */
/*==================[Definicion funciones de configuración]==========================*/

static void initHardware(void)  {
	Board_Init();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock / MILISEC);		//systick 1ms

	/* Inicializar UART_USB a 115200 baudios */
	uartConfig( UART_USB, 115200 );
}

void configInterrupts(void){
	Chip_PININT_Init(LPC_GPIO_PIN_INT);
	//--- Mapeo entre el pin fisico y el canal del handler que atendera la interrupcion
	Chip_SCU_GPIOIntPinSel(0, 0, 4); //-- TEC1
	Chip_SCU_GPIOIntPinSel(1, 0, 8); //-- TEC2

	//-- Configuro interrupcion TEC1
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);//-- Configuracion de interrupcion por flanco
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);//-- Flanco descendente
	Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH0);//-- Flanco ascendente

	//-- El SO es el encargado de instalar la IRQ
	os_InstalarIRQ(PIN_INT0_IRQn,TEC1_IRQHandler);

	//-- Configuro interrupcion TEC2
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH1);//-- Configuracion de interrupcion por flanco
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH1);//-- Flanco descendente
	Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH1);//-- Flanco ascendente

	//-- El SO es el encargado de instalar la IRQ
	os_InstalarIRQ(PIN_INT1_IRQn,TEC2_IRQHandler);

	//-- Creo los semaforos que voy a utilizar en cada tecla para cada flanco
	os_SemaforoInit(&SemBinTec1RiseEdge);
	os_SemaforoInit(&SemBinTec1FallEdge);
	os_SemaforoInit(&SemBinTec2RiseEdge);
	os_SemaforoInit(&SemBinTec2FallEdge);
	//-- Por defecto inician tomados por lo cual estan listos para ser liberados por los handlers.
}
/*==================[Definicion de tareas para el OS]==========================*/

void t_estadoTec1(void){

	tec_frame_t data_tec1;
	data_tec1.tec = 1;
	while (1){
		//-- Tomamos el semaforo - el semaforo solo puede ser tomado si
		//-- un flanco descendente se produjo.
		//-- SemaforoTake se le puede ingresar timeout pero en este caso
		//-- no nos es util por lo cual se deshabilita poniendo -1

		if(os_SemaforoTake(&SemBinTec1FallEdge,-1)){
			gpioWrite(LED2,true);
			data_tec1.t_fall  = os_getTickCounter();
		}

		//-- Tomamos el semaforo - el semaforo solo puede ser tomado si
		//-- un flanco ascendente se produjo.
		//-- En este caso tambien utilizamos -1 para indicar que siempre esté
		//-- bloqueado.
		if(os_SemaforoTake(&SemBinTec1RiseEdge,-1)){
			gpioWrite(LED2,false);
			data_tec1.t_rise  = os_getTickCounter();
		}

		//-- Se envìa por una cola de mensajes los valores obtenidos
		os_ColaWrite(&colaTecData,&data_tec1);
	}
}
void t_estadoTec2(void){

	tec_frame_t data_tec2;
	data_tec2.tec = 2;
	while (1){
		//-- Tomamos el semaforo - el semaforo solo puede ser tomado si
		//-- un flanco descendente se produjo.
		//-- SemaforoTake se le puede ingresar timeout pero en este caso
		//-- no nos es util por lo cual se deshabilita poniendo -1

		if(os_SemaforoTake(&SemBinTec2FallEdge,-1)){
			gpioWrite(LED3,true);
			data_tec2.t_fall = os_getTickCounter();
		}

		//-- Tomamos el semaforo - el semaforo solo puede ser tomado si
		//-- un flanco ascendente se produjo.
		//-- En este caso tambien utilizamos -1 para indicar que siempre esté
		//-- bloqueado.
		if(os_SemaforoTake(&SemBinTec2RiseEdge,-1)){
			gpioWrite(LED3,false);
			data_tec2.t_rise = os_getTickCounter();
		}

		//-- Se envìa por una cola de mensajes los valores obtenidos
		os_ColaWrite(&colaTecData,&data_tec2);
	}
}

void t_processor(void){
	tec_frame_t data_tec_actual,data_tec_anterior;
	tec_frame_t tec1data, tec2data;
	led_frame_t data_led;
	data_time_frame_t data_times;
	bool validando;

	 while (1){
		 //-- Antes de un nuevo análisis se borra el valor anterior
		 data_tec_anterior.t_fall = 0;
		 data_tec_anterior.t_rise = 0;
		 data_tec_anterior.tec = 0;

		 //-- Se espera hasta que ambas colas envíen sus tiempos
		 //-- Se tienen en cuenta los tiempos de la última vez que
		 //-- se pulsaron las teclas.
		 validando = true;
		 while(validando){
			 os_ColaRead(&colaTecData,&data_tec_actual);
			 //-- Debemos verificar que la recepciòn actual posea un dato de una tecla
			 //-- distinta a la anterior.
			 if((data_tec_actual.tec != data_tec_anterior.tec) &&(data_tec_anterior.tec != 0)){
				 //-- Si las teclas son distintas puede suceder que la tecla anterior fue
				 //-- presionada y soltada en un tiempo previo a cuando fue presionada la
				 //-- tecla actual. En ese caso el evento debe ignorarse y solo se
				 //-- guarda los datos de la tecla actual en la anterior
				 if(data_tec_actual.t_fall > data_tec_anterior.t_rise){
					 data_tec_anterior = data_tec_actual;
				 }
				 else{
					 //-- En este caso tengo el estado de las dos teclas y ambos fueron
					 //-- sucesos concurrentes, es decir, el tiempo de rise de una tecla
					 //-- se encuentra incluedo en el periodo fall rise de la otra.
					 validando = false;
				 }
			 }
			 else{
				 //-- Si la recepciòn actual corresponde a la misma tecla que la
				 //-- recepciòn anterior, se debe actualizar la anterior y esperar
				 //-- una nueva recepciòn.
				 data_tec_anterior = data_tec_actual;
			 }

		 }
		 //-- Para mayor legibilidad del codigo se guardan los valores de las estructuras
		 //-- obtenidas en 2 estructuras tec1data y tec2data
		 if(data_tec_actual.tec ==1){
			 tec1data = data_tec_actual;
			 tec2data = data_tec_anterior;
		 }
		 else{
			 tec2data = data_tec_actual;
			 tec1data = data_tec_anterior;
		 }

		 //-- Comparacion de tiempos descendentes entre tecla1 y tecla2
		 if(tec1data.t_fall > tec2data.t_fall){
			 //-- Esta condicion la cumple solo el caso del LED AMARILLO o el LED AZUL
			 data_times.t1 = tec1data.t_fall - tec2data.t_fall;

			 //-- Comparacion de tiempos ascendentes entre tecla1 y tecla2
			 if(tec1data.t_rise > tec2data.t_rise){
				 //-- El led AZUL debe ser encendido.
				 data_times.t2 = tec1data.t_rise - tec2data.t_rise;
				 data_times.ledx = LEDB;
			 }
			 else{
				 //-- El led AMARILLO debe ser encendido.
				 data_times.t2 = tec2data.t_rise - tec1data.t_rise;
				 data_times.ledx = LED1;
			 }
		 }
		 else{
			 //-- Esta condicion la cumple solo el caso del LED VERDE o el LED ROJO
			 data_times.t1 = tec2data.t_fall - tec1data.t_fall;

			 //-- Comparacion de tiempos ascendentes entre tecla1 y tecla2
			 if(tec1data.t_rise > tec2data.t_rise){
				 //-- El led ROJO debe ser encendido.
				 data_times.t2 = tec1data.t_rise - tec2data.t_rise;
				 data_times.ledx = LEDR;
			 }
			 else{
				 //-- El led VERDE debe ser encendido.
				 data_times.t2 = tec2data.t_rise - tec1data.t_rise;
				 data_times.ledx = LEDG;
			 }
		 }
		 //-- Cargo los demas valores dentro de las estructuras para ser enviadas a las demas
		 //-- tareas mediante las colas
		 data_times.time_on = data_times.t1 + data_times.t2;
		 data_led.time_on = data_times.time_on;
		 data_led.ledx = data_times.ledx;

		 os_ColaWrite(&colaEncLedData,&data_led);
		 os_ColaWrite(&colaDataMsg,&data_times);

	 }
}

void t_armarMsg(void){
	data_time_frame_t data_frame;
	char strEnvio[55],strDato[10];
	char* ptr_strEnvio = NULL;
	uint8_t i, index;

	while (1){

		//-- Si llega un frame de datos por la cola estoy en condiciones
		//-- de armar el texto que será enviado por la uart.
		os_ColaRead(&colaDataMsg,&data_frame);
		os_Delay(299);
		for(index = 1; index<=4; index++){
			if(index==1){
				if(data_frame.ledx == LEDR){
					strcpy(strEnvio,"Led ROJO encendido:\n\r");
				}
				else if(data_frame.ledx == LEDG){
					strcpy(strEnvio,"Led VERDE encendido:\n\r");
				}
				else if(data_frame.ledx == LEDB){
					strcpy(strEnvio,"Led AZUL encendido:\n\r");
				}
				else if(data_frame.ledx == LED1){
					strcpy(strEnvio,"Led AMARILLO encendido:\n\r");
				}
				else{
					strcpy(strEnvio,"Error lectura de led\n\r");
				}

			}
			else if(index==2){
				strcpy(strEnvio, "\t Tiempo encendido: " );
				strcpy(strDato, integerToString(data_frame.time_on,strDato,10));
				strcat(strEnvio, strDato);
				strcat(strEnvio, "ms \r\n");
			}
			else if(index==3){
				strcpy(strEnvio, "\t Tiempo entre flancos descendentes: " );
				strcpy(strDato, integerToString(data_frame.t1,strDato,10));
				strcat(strEnvio, strDato);
				strcat(strEnvio, "ms \r\n");
			}
			else{
				strcpy(strEnvio, "\t Tiempo entre flancos ascendentes: " );
				strcpy(strDato, integerToString(data_frame.t2,strDato,10));
				strcat(strEnvio, strDato);
				strcat(strEnvio, "ms \r\n\r\n\r\n");
			}

			//-- Envìo el puntero del string para que se cree una copia local
			//-- en uartSend y se pueda esperar al procesamiento de un nuevo
			//-- dato de la cola  colaDataMsg
			ptr_strEnvio = strEnvio;
			os_ColaWrite(&colaUart,&ptr_strEnvio);
			os_SemaforoTake(&SemBinMsgTransmit,-1);

		}

	}
}

void t_uartSend(void){

	char* ptr_string= NULL;
	char strRcv[55];
	uint8_t i;
	while(1)  {

		os_ColaRead(&colaUart,&ptr_string);

		strcpy(strRcv, ptr_string );
		os_SemaforoGive(&SemBinMsgTransmit);
		i=0;
		for(i=0; i< strlen(strRcv); i++){
			uartWriteByte(UART_USB,strRcv[i]);
		}
		//uartWriteByte(UART_USB,caracter);
	}
}

void t_encenderLed(void){

	led_frame_t aux_data_led;
	 while (1){
		 os_ColaRead(&colaEncLedData,&aux_data_led);
		 gpioWrite(aux_data_led.ledx,true);
		 os_Delay(aux_data_led.time_on);
		 gpioWrite(aux_data_led.ledx,false);
	 }
}

/*==================[Definicion de handlers de interrupciones]==========================*/
void TEC1_IRQHandler(void){

	//-- Detectamos si se produjo por un flanco descendente
	if( Chip_PININT_GetFallStates(LPC_GPIO_PIN_INT) & PININTCH0){
		//-- Limpiamos el flag relacionado con la interrupciòn
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT,PININTCH0);
		//-- Liberamos el semaforo relacionado con éste evento
		os_SemaforoGive(&SemBinTec1FallEdge);
	}
	//-- o si se produjo por un flanco ascendente
	else if( Chip_PININT_GetRiseStates(LPC_GPIO_PIN_INT) & PININTCH0){
		//-- Limpiamos el flag relacionado con la interrupciòn
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT,PININTCH0);
		//-- Liberamos el semaforo relacionado con éste evento
		os_SemaforoGive(&SemBinTec1RiseEdge);
	}

}
void TEC2_IRQHandler(void){

	//-- Detectamos si se produjo por un flanco descendente
	if( Chip_PININT_GetFallStates(LPC_GPIO_PIN_INT) & PININTCH1){
		//-- Limpiamos el flag relacionado con la interrupciòn
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT,PININTCH1);
		//-- Liberamos el semaforo relacionado con éste evento
		os_SemaforoGive(&SemBinTec2FallEdge);
	}
	//-- o si se produjo por un flanco ascendente
	else if( Chip_PININT_GetRiseStates(LPC_GPIO_PIN_INT) & PININTCH1){
		//-- Limpiamos el flag relacionado con la interrupciòn
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT,PININTCH1);
		//-- Liberamos el semaforo relacionado con éste evento
		os_SemaforoGive(&SemBinTec2RiseEdge);
	}

}
/*============================================================================*/

int main(void)  {

	initHardware();

	os_InitTarea(t_estadoTec1, &task_estadoTec1,PRIORIDAD_0);
	os_InitTarea(t_estadoTec2, &task_estadoTec2,PRIORIDAD_0);

	os_InitTarea(t_processor, &task_processor,PRIORIDAD_1);
	os_InitTarea(t_armarMsg, &task_armarMsg,PRIORIDAD_1);

	os_InitTarea(t_uartSend, &task_uartSend,PRIORIDAD_2);
	os_InitTarea(t_encenderLed, &task_encenderLed,PRIORIDAD_2);

	configInterrupts();

	os_SemaforoInit(&SemBinMsgTransmit);

	os_ColaInit(&colaUart,sizeof(char *));
	os_ColaInit(&colaEncLedData,sizeof(led_frame_t));
	os_ColaInit(&colaTecData,sizeof(tec_frame_t));
	os_ColaInit(&colaDataMsg,sizeof(data_time_frame_t));

	os_Init();

	while (1) {
	}
}



/** @} doxygen end group definition */

/*==================[end of file]============================================*/
