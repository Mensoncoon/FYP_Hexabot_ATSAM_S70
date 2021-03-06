/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */
#include <Hexabot/Hexabot.h>
#include <asf.h>
#include "../Debug.h"
#include "DW1000.h"
#include <Hexabot/Hexabot_Cmd.h>
#include <Hexabot/Gait.h>
#include <arm_math.h>

#define CAM_DIF_TSH cam_dif_tsh

char buf [2000];
#if TX_TEST_MODE == 0
char RXbuf[1024];
#else
char RXbuf[] = "WIRELESS TEST ABCD 1234";
#endif

//define task functions
void vTask1 (void*);
void LegControlTask (void*);
void CLItask(void*);
void ImageProTask(void*);
void WirelessTask(void*);
uint16_t* intl_frame;
//global variables

int resting = 1;
//frame pointer
int isi_frames_done = 0;
//pixel difference
int diffPix = 0;
//CLI buffer
char CLIbuf[100];
//CLIbuffer indes
int CLIbufIndex = 0;
//gait and walk settings and status
walk_data hexabot_walk;
//debug spitout mode
int VerboseMode = 0;
//pixel difference requiremet
int cam_dif_tsh = 25;
//button up varuable
int But_Up = 0;
//Servo calibration array
float SvoCal[] = {2.190468,2.048677,-8.911903,1.801958,-6.651421,1.852191,-3.200684,1.928874,-11.552467,1.743279,-13.162811,1.707493,0.919999,0.020444,-2.519998,-0.056000,-2.289998,-0.050889,-4.069998,-0.090444,-4.430007,-0.098445,-2.149998,-0.047778,3.470741,1.038564,-7.911690,0.912092,-0.450096,0.994999,-3.880829,0.956880,11.532463,1.128138,-11.432442,0.872973};


int UART_Ctrl_EN = 0;
int UART_Ctrl_Cnt = 0;
int UART_Ctrl_Max = 0;
int ButtonStatus;


//semaphores!
SemaphoreHandle_t ISIsem = NULL;
SemaphoreHandle_t UARTsem = NULL;
SemaphoreHandle_t WIRELESSsem = NULL;

int main (void)
{
		
	
	board_init();
	sendDebugString("BOARD INITIALIZATION - FINISHED\n");
	intl_frame = (uint16_t*)malloc(240*320*2); //assign
	//SvoCal = (float*)malloc(sizeof(float)*2*18);
	
	//for(int i = 0;i<36;i++) SvoCal[i] = 0;
	//for(int i = 0;i<36;i++) SvoCal[i] = 0;
	
	sendDebugString("RTOS TASK INITIALIZATION - STARTED\n");
	
	xTaskCreate(vTask1,"TASK1",400,NULL,10,NULL);
#if TX_TEST_MODE == 0
	xTaskCreate(LegControlTask,"LEGCTRLTASK",2000,NULL,5,NULL);
	//xTaskCreate(ImageProTask,"IMGTASK",2000,NULL,4,NULL);
	xTaskCreate(CLItask,"CLITASK",2500,NULL,6,NULL);
	xTaskCreate(WirelessTask,"WIRELESSTASK",2500,NULL,3,NULL);
#endif
	sendDebugString("RTOS TASK INITIALIZATION - FINISHED\n");
	
	sendDebugString("STARTING RTOS\n");
	vTaskStartScheduler();
	sendDebugString("RTOS HAS RETURNED. THIS SHOULD EVER HAPPEN. EXTREME ERROR\n");
	return 0;
	/* Insert application code here, after the board has been initialized. */
}

void vTask1 (void* pvParameters) {
	sendDebugString("TASK1 INITIALIZATION - STARTED\n");
	TickType_t xLastWakeTime = xTaskGetTickCount();
	int tg = 1;
	uint8_t T = 0;
	int cleanTest = 1;
	int testCountPass = 0;
	char Qbuf[20];
	char buf[20];
	pio_set(LED0);
	int testCountFail = 0;
	
	int batLowCount = 0;
	
	sendDebugString("TASK1 INITIALIZATION - FINISHED | ENTERING INFINITE LOOP\n");
	for(;;) {
				if(tg) {
					pio_set(LED0);
#if TX_TEST_MODE == 0
					if(!hexabot_walk.Walk_EN && getBatVoltage() < 6.25 && !pio_get(PIOD,PIO_INPUT,1<<9))  batLowCount++;
					else batLowCount = 0;
					if(batLowCount > 10) {
						sendDebugString("********************\n");
						sendDebugString("********************\n");
						sendDebugString("WARNING: BAT VOLT AT CRITICAL LEVELS\n");
						sendDebugString("DISABLING SERVO WRITE OUT\n");
						sendDebugString("PLEASE DISCONNECT BATTERY NOW!\n");
						sendDebugString("********************\n");
						sendDebugString("********************\n");
						pio_set(PIOA,PIO_PA26);
						
						pio_set(LED0);
						pio_set(LED1);
						pio_set(LED2);
						pio_set(LED3);
						pio_set(LED4);
						pio_set(LED5);
						pio_set(LED6);
						pio_set(LED7);
					}
#else
					
					cmdDWMsend(RXbuf,strlen(RXbuf)+2);
#endif
					tg = !tg;
				}
				else {
					pio_clear(LED0);
#if TX_TEST_MODE == 0
					if(!hexabot_walk.Walk_EN && getBatVoltage() < 6.25) {
						pio_clear(LED0);
						pio_clear(LED1);
						pio_clear(LED2);
						pio_clear(LED3);
						pio_clear(LED4);
						pio_clear(LED5);
						pio_clear(LED6);
						pio_clear(LED7);
					}
#endif
					tg = !tg;
				}	
				vTaskDelay(250);
	}
}

void LegControlTask (void* pvParameters) {
	sendDebugString("LEG CONTROL TASK INITIALIZATION - STARTED\n");
	
	float	ofst[7];
	XZ		xzS[7];
	angles	Ang[7];
	
	hexabot_walk.movTurn = 0;
	hexabot_walk.movDir = 0;
	hexabot_walk.stance = 165;
	hexabot_walk.hgt = 20;
	hexabot_walk.pup = 85;
	hexabot_walk.stride = 65;
	hexabot_walk.Walk_EN = 0;
	hexabot_walk.Hexabot_leg_cycle_t = 150;
	hexabot_walk.ret = 0;
	hexabot_walk.gaitIndex = 2;
	hexabot_walk.standing = 1;
	
	cmdServoMan(6,0,90.00);
	cmdServoMan(6,1,0.00);
	cmdServoMan(6,2,90.00);
		
	sendDebugString("LEG CONTROL TASK INITIALIZATION - FINISHED | ENTERING INFINITE LOOP\n");
	
	for(;;) {
		pio_set(LED7);
		if(hexabot_walk.Walk_EN) {
			
		switch(hexabot_walk.gaitIndex) {
			
			case 0:	
			Gait0(ofst,xzS,Ang,(walk_data*) &hexabot_walk);
			break;
			
			case 1:
			Gait1(ofst,xzS,Ang,(walk_data*) &hexabot_walk);
			break;
			
			case 2:
			Gait2(ofst,xzS,Ang,(walk_data*) &hexabot_walk);
			break;
			
			case 98:
			sitDown(ofst,xzS,Ang,(walk_data*) &hexabot_walk);
			break;
			
			case 99:
			standUp(ofst,xzS,Ang,(walk_data*) &hexabot_walk);
			break;
		}
		  
		  writeLegOut(0,Ang[0].S1,Ang[0].S2,Ang[0].S3);
		  writeLegOut(1,Ang[1].S1,Ang[1].S2,Ang[1].S3);
		  writeLegOut(2,Ang[2].S1,Ang[2].S2,Ang[2].S3);
		  writeLegOut(3,Ang[3].S1,Ang[3].S2,Ang[3].S3);
		  writeLegOut(4,Ang[4].S1,Ang[4].S2,Ang[4].S3);
		  writeLegOut(5,Ang[5].S1,Ang[5].S2,Ang[5].S3);
		  hexabot_walk.i++;
		 hexabot_walk. ret = 1;
		  if(hexabot_walk.i > hexabot_walk.max_i) hexabot_walk.Walk_EN = 0;
		  
		}
		else {
			if(hexabot_walk.ret){
				if(!resting) {
		  xzS[0] = calcRotation(hexabot_walk.stance, 0, hexabot_walk.stance, 0, 0,1,0);
		  xzS[1] = calcRotation(hexabot_walk.stance, 0, hexabot_walk.stance, 0, 0,0,0);
		  xzS[2] = calcRotation(hexabot_walk.stance, 0, hexabot_walk.stance, 0, 0,1,0);
		  xzS[3] = calcRotation(hexabot_walk.stance, 0, hexabot_walk.stance, 0, 0,0,0);
		  xzS[4] = calcRotation(hexabot_walk.stance, 0, hexabot_walk.stance, 0, 0,1,0);
		  xzS[5] = calcRotation(hexabot_walk.stance, 0, hexabot_walk.stance, 0, 0,0,0);

		  Ang[0] = legAngCalc(xzS[0].X,  -hexabot_walk.hgt  ,xzS[0].Z);
		  Ang[1] = legAngCalc(xzS[1].X,  -hexabot_walk.hgt  ,xzS[1].Z);
		  Ang[2] = legAngCalc(xzS[2].X,  -hexabot_walk.hgt  ,xzS[2].Z);
		  Ang[3] = legAngCalc(xzS[3].X,  -hexabot_walk.hgt  ,xzS[3].Z);
		  Ang[4] = legAngCalc(xzS[4].X,  -hexabot_walk.hgt  ,xzS[4].Z);
		  Ang[5] = legAngCalc(xzS[5].X,  -hexabot_walk.hgt  ,xzS[5].Z);
		  
		  writeLegOut(0,Ang[0].S1,Ang[0].S2,Ang[0].S3);
		  writeLegOut(1,Ang[1].S1,Ang[1].S2,Ang[1].S3);
		  writeLegOut(2,Ang[2].S1,Ang[2].S2,Ang[2].S3);
		  writeLegOut(3,Ang[3].S1,Ang[3].S2,Ang[3].S3);
		  writeLegOut(4,Ang[4].S1,Ang[4].S2,Ang[4].S3);
		  writeLegOut(5,Ang[5].S1,Ang[5].S2,Ang[5].S3);
				}
		  hexabot_walk.ret = 0;
		  if(hexabot_walk.gaitIndex == 99 || hexabot_walk.gaitIndex == 98) hexabot_walk.gaitIndex = 2;
		}
			hexabot_walk.i = 0;
			//return to idle state (legs in middle) 
		}
		pio_clear(LED7);
		  vTaskDelay(5);
	}
}

void CLItask(void* pvParameters) {
	sendDebugString("CLI TASK INITIALIZATION - STARTED\n");
	memset(CLIbuf,0,100);
	char* BaseCmd;
	sendDebugString("CLI TASK INITIALIZATION - FINISHED | ENTERING INFINITE LOOP\n");
	sendDebugString("COMMAND LINE STARTED\n");
	sendDebugString("\n");
	sendDebugString("\n");
	UARTsem = xSemaphoreCreateBinary();
	
	
	sendDebugString("FYP_Hexabot_ATSAMS70_MELLATRON9000>");
	for(;;) {
			if(xSemaphoreTake(UARTsem,0xFFFF) == pdTRUE) {
		if(CLIbuf[CLIbufIndex-1] ==  '\n') {
			CLIbufIndex=0;
			//sendDebugString(CLIbuf);
			BaseCmd = strtok(CLIbuf," ");
			
			if(UART_Ctrl_EN) {
				UART_Ctrl_EN = 0;
				UART_Ctrl_Cnt = 0;
				pio_clear(LED6);
				cmdInterp(CLIbuf,UART_Ctrl_Max,&hexabot_walk);
			}
			else { 
			if(!strcmp(BaseCmd,"led")) cmdLED( atoi(strtok(NULL," "))  , atoi(strtok(NULL," ")) );
			
			else if(!strcmp(BaseCmd,"manusvo"))  cmdServoMan(atoi(strtok(NULL," ")) , atoi(strtok(NULL," ")) , atoi(strtok(NULL," ")));
			
			else if(!strcmp(BaseCmd,"batvolt\n")) cmdBatVolt();
			
			else if(!strcmp(BaseCmd,"dumpimg")) dumpFrame( strtol(strtok(NULL," "),NULL,16));
			
			else if(!strcmp(BaseCmd,"walk")) cmdWalk(atoi(strtok(NULL," ")));
			
			else if(!strcmp(BaseCmd,"walkcytime")) hexabot_walk.Hexabot_leg_cycle_t = atoi(strtok(NULL," "));
			
			else if(!strcmp(BaseCmd,"verbose")) VerboseMode = atoi(strtok(NULL," "));
			
			
			//DWM THINGS
			else if(!strcmp(BaseCmd,"DWM-reset\n")) {
				 	pio_set(PIOD,PIO_PD10);
				 	delay_ms(100);
				 	pio_clear(PIOD,PIO_PD10);
			 }
			
			else if(!strcmp(BaseCmd,"DWM-test\n")) cmdTestDW1000();
			
			else if(!strcmp(BaseCmd,"DWM-send")){
				char* temp = strtok(NULL," ");
				 cmdDWMsend(temp,strlen(temp));
			}
			
			else if(!strcmp(BaseCmd,"DWM-orLed\n")) cmdOverrideLEDDWM1000();
			
			else if(!strcmp(BaseCmd,"DWM-RWtest")) cmdWriteTestDW1000( strtol(strtok(NULL," "),NULL,16));
			
			else if(!strcmp(BaseCmd,"DWM-Init\n")) DW1000_initialise();
			
			else if(!strcmp(BaseCmd,"DWM-LEDinit\n")) DW1000_toggleGPIO_MODE();
			
			else if(!strcmp(BaseCmd,"DWM-RXEN\n")) cmdRXen();
			
			else if(!strcmp(BaseCmd,"DWM-ReadRX\n")) {
						
				////uint64_t msgLen = cmdDWMreadRX(RXbuf);
				//int cunt = 0;
				//cunt = DW1000_readReg(RX_BUFFER_ID, DW1000_NO_SUB,0,4);
				//sprintf(buf,"Length: 0x%016x\n",cunt);
				//sendDebugString(buf);
				//
				////sprintf(buf,"SomeData: %s\n",RXbuf);
				////sendDebugString(buf);
				//
				
				//memset(RXbuf,0,1024);
				uint64_t msgLen = cmdDWMreadRX(RXbuf);
				
				sprintf(buf,"Length: %d\n",msgLen);
				sendDebugString(buf);
				
				sprintf(buf,"SomeData: %s\n",RXbuf);
				sendDebugString(buf);
				
				
			}
			
			//END OF DWM THINGS
			
			
			
			else if(!strcmp(BaseCmd,"camdtsh")) cam_dif_tsh = atoi(strtok(NULL," "));
			
			else if(!strcmp(BaseCmd,"memtest\n")) SdramCheck();
			
			else if(!strcmp(BaseCmd,"DWM-clrStatus\n")) DW1000_writeReg(SYS_STATUS_ID, DW1000_NO_SUB, DW1000_NO_OFFSET, 0xFFFFFFFF, SYS_STATUS_LEN);
			
			else if(!strcmp(BaseCmd,"svoCal\n")) calibServos(SvoCal);
			
			else if(!strcmp(BaseCmd,"svoCalSpec")) calibServoSpec(SvoCal,atoi(strtok(NULL," ")),atoi(strtok(NULL," ")));
			
			else if(!strcmp(BaseCmd,"surprise\n")) surprise();
			
			//walk patern settings
			else if(!strcmp(BaseCmd,"relaxSvo")) cmdRelaxSvo(atoi(strtok(NULL," ")) , atoi(strtok(NULL," ")));
			
			else if(!strcmp(BaseCmd,"StandUp\n")) {
				if(resting != 0){
					hexabot_walk.gaitIndex = 99;
					hexabot_walk.i =0;
					hexabot_walk.max_i = STAND_UP_TIME;
					hexabot_walk.Walk_EN = 1;
					resting = 0;
				}
			}
			
			else if(!strcmp(BaseCmd,"SitDown\n")) {
				if(resting != 1){
					hexabot_walk.gaitIndex = 98;
					hexabot_walk.i =0;
					hexabot_walk.max_i = STAND_UP_TIME;
					hexabot_walk.Walk_EN = 1;
					resting = 1;
				}
				
			}
			
			else if(!strcmp(BaseCmd,"relaxAll\n")) cmdRelaxAll();
			//walk patern settings
			
			else if(!strcmp(BaseCmd,"gaitTurn")){
				hexabot_walk.movTurn = atoff(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			else if(!strcmp(BaseCmd,"gaitDir")){
				hexabot_walk.movDir = atoff(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			else if(!strcmp(BaseCmd,"gaitStance")){
				hexabot_walk.stance = atoi(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			else if(!strcmp(BaseCmd,"gaitHgt")){
				hexabot_walk.hgt = atoi(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			else if(!strcmp(BaseCmd,"gaitPup")){
				hexabot_walk.pup = atoi(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			else if(!strcmp(BaseCmd,"gaitStride")){
				hexabot_walk.stride = atoi(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			else if(!strcmp(BaseCmd,"gaitStyle")){
				hexabot_walk.gaitIndex = atoi(strtok(NULL," "));
				hexabot_walk.ret = 1;
			}
			
			else if(!strcmp(BaseCmd,"svoinhib")) {
				if(atoi(strtok(NULL," "))) pio_set(PIOA,PIO_PA26);
				else pio_clear(PIOA,PIO_PA26);
			}
			
			//controller command
			else if(!strcmp(BaseCmd,"ctrlCmd")) {
				UART_Ctrl_EN = 1;
				UART_Ctrl_Max = atoi(strtok(NULL," "));
				pio_set(LED6);
			}
			
			else if(!strcmp(BaseCmd,"RESET\n")) rstc_start_software_reset(RSTC);
			
			else sendDebugString("ERROR: Command not found\n");
			
			sendDebugString("FYP_Hexabot_ATSAMS70_MELLATRON9000>");
			}
			memset(CLIbuf,0,100);
			//Figure out function, then commit;
			}
		}
	}
}

void ImageProTask(void* pvParams) {
	sendDebugString("CAMERA PROCESSING TASK INITIALIZATION - STARTED\n");
	
	ISIsem = xSemaphoreCreateBinary();
	isi_enable_interrupt(ISI,1<<16|1<<17);
	NVIC_ClearPendingIRQ(ISI_IRQn);
	NVIC_SetPriority(ISI_IRQn,7);
	NVIC_EnableIRQ(ISI_IRQn);
	sendDebugString("CAMERA PROCESSING TASK INITIALIZATION - FINISHED\n");
	volatile uint16_t * frame0 = (uint16_t*)CAM_FRAME0_ADDR;
	volatile uint16_t * frame1 = intl_frame;
	volatile uint16_t * dif1 = (uint16_t*)0x71000000;
	for(;;) {
		if(xSemaphoreTake(ISIsem,0xFFFF) == pdTRUE) {
				//do dif here	
				pio_set(LED3);
				diffPix=0;
				uint16_t tempframe0 = 0;
				uint16_t tempframe1 = 0;
				for(int i = 0; i<320*240;i++) {
					tempframe0 = frame0[i];
					tempframe1 = frame1[i];
					
					if( (tempframe1&0xF800 > tempframe0&0xF800)?( ((tempframe1&0xF800)>>11) - ((tempframe0&0xF800)>>11) ):( ((tempframe0&0xF800)>>11) - ((tempframe1&0xF800)>>11) ) > CAM_DIF_TSH ||
						(tempframe1&0x07e0 > tempframe0&0x07e0)?( ((tempframe1&0x07e0)>>5) - ((tempframe0&0x07e0)>>5) ):( ((tempframe0&0x07e0)>>5) - ((tempframe1&0x07e0)>>5) ) > CAM_DIF_TSH ||
						(tempframe1&0x001F > tempframe0&0x001F)?( ((tempframe1&0x001F)>>0) - ((tempframe0&0x001F)>>0) ):( ((tempframe0&0x001F)>>0) - ((tempframe1&0x001F)>>0) ) > CAM_DIF_TSH ) {
							dif1[i] = tempframe0;	
							diffPix++;
						}
						else dif1[i] = 0x0000;
						//for(int d =0;d<20;d++) ((volatile uint16_t*)(0x7F000000))[0] = 0x0000; //for(int d =0;d<50;d++)	asm volatile ("nop");
						//((volatile uint8_t*)frame1)[2*i] = (tempframe0&0xFF00)>>8;
						//((volatile uint8_t*)frame1)[2*i+1] = tempframe0&0x00FF;
						frame1[i] = tempframe0;
						
						//for(int d =0;d<20;d++)	asm volatile ("nop");
				}
				//for(int i = 0; i<320*240*2;i++) ((uint8_t*)frame1)[i] = ((uint8_t*)frame0)[i];
				if(VerboseMode){
					sprintf(buf,"changed pix: %d\nbandwith: %f\%\n",diffPix,( ((float)diffPix*2.0)/(320.00*240.00))*100 );
					sendDebugString(buf);
				}		
				pio_clear(LED3);
		}
	}
}

void WirelessTask(void* pvParams) {
	WIRELESSsem = xSemaphoreCreateBinary();
	cmdRXen();
	for(;;) {
		if(xSemaphoreTake(WIRELESSsem,0xFFFF) == pdTRUE) {
		pio_set(LED2);
		uint64_t msgLen = cmdDWMreadRX(RXbuf);	
		
		sendDebugString("Code Received 0x");
		for(int i = 0; i < msgLen; i++) {
		sprintf(buf,"%02x",RXbuf[i]);
		sendDebugString(buf);
		}
		sendDebugString("\n");
		cmdInterp(RXbuf,msgLen,&hexabot_walk);
		cmdRXen();
		pio_clear(LED2);
		}
	}
}


	/* ######################################
	   ######################################
			 	INTERUPT HANDLERS
	   ######################################
	   ###################################### */
	
void ISI_Handler(void) {
	uint32_t status,imr;
	status = ISI->ISI_SR;
	imr = ISI->ISI_IMR;
	//pio_set(LED3);
	isi_frames_done++;
	if(isi_frames_done >= 1) {
		xSemaphoreGiveFromISR(ISIsem,NULL);
		isi_frames_done = 0;
	}
}


void UART4_Handler(void) {
	uint32_t imr = ISI->ISI_IMR;
	char temp;
	uart_read(UART4,&temp);
	CLIbuf[CLIbufIndex] = temp;
	CLIbufIndex++;
	
	if(UART_Ctrl_EN) {
		UART_Ctrl_Cnt++;
		if(UART_Ctrl_Max <= UART_Ctrl_Cnt) {
			xSemaphoreGiveFromISR(UARTsem,NULL);
		}
	}
	else {
	if(temp == 10){
		 xSemaphoreGiveFromISR(UARTsem,NULL);
		}
	}
}

void PIOA_Handler (void) {
	ButtonStatus = pio_get_interrupt_status(PIOA);
	ButtonStatus &= pio_get_interrupt_mask(PIOA);
	xSemaphoreGiveFromISR(WIRELESSsem,NULL);
}