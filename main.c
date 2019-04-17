/*
 *  ======== main.c ========
 */
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

/* TI-RTOS Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>


// Vakio: sensorin datarekisterin osoite
// Tämä arvo saadaan datakirjasta (ts. kurssilla annetaan materiaalissa)
// Hox! Tämä arvo ei siis ole sensorin i2c-osoite! Hetki..
#define TMP007_REG_TEMP     0x0003 

/* Board Header files */
#include "Board.h"
#include "sensors/mpu9250.h"
#include "sensors/bmp280.h"

/* JTKJ Header files */
#include "wireless/comm_lib.h"

/* Task Stacks */
#define STACKSIZE 2048
Char labTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];
Char sensorTaskStack[STACKSIZE];

/* JTKJ: Display */
Display_Handle hDisplay;

/* Global variables */

//Sensorialgoritmin käyttämät globaalit muuttujat
float keskidata[10];
int datalaskuri = 0;
int hissilaskuri = 0;
int hissivirhe = 0;

//muut globaalit muuttujat
int askeleet = 0;
int portaat = 0;
int hissit = 0;
int valikkoruutu = 1;
int valinta = 1;

//Viestinnän globaalit muuttujat
char sendload[16]; //lähtevien viestien puskuri
char payload[16]; // saapuvien viestien puskuri
	
/* Pin juttuja, joille tehdään jotakin */
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU9250 uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

/* JTKJ: Pin Button1 configured as power button */
static PIN_Handle hPowerButton;
static PIN_State sPowerButton;
PIN_Config cPowerButton[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};
PIN_Config cPowerWake[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE
};

/* JTKJ: Pin Button0 configured as input */
static PIN_Handle hButton0;
static PIN_State sButton0;
PIN_Config cButton0[] = {
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // JTKJ: CONFIGURE BUTTON 0 AS INPUT (SEE LECTURE MATERIAL)
    PIN_TERMINATE
};

/* JTKJ: Leds */
static PIN_Handle hLed;
static PIN_State sLed;
PIN_Config cLed[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, // JTKJ: CONFIGURE LEDS AS OUTPUT (SEE LECTURE MATERIAL)
    PIN_TERMINATE
};

/* Prototyypit */
void sensorialgoritmi(float ax, float ay, float az);
void tyhjennakeskidata(void);

/* Funktiot */
void sensorialgoritmi(float ax, float ay, float az) {
    
    float vec;
    float hissivec = 1.00;
    float keskiarvo;
    
    //Lasketaan ax, ay ja az muodostaman kolmiulotteisen vektorin pituus
    vec = sqrt((ax*ax) + (ay*ay) + (az*az));
    
    //Askeleiden, portaiden tunnistus ja hissimatkan tuottamien vektoreiden karsiminen muista
    if (vec >= 1.2) {
            askeleet++;
            hissivec = 0.0;
        }
        if (vec >= 1.34) {
            portaat++;
        }
        if (vec <= 1.16) {
            hissivec = vec;
        }
        
    //Hissintunnistusta varten lasketaan 10 viimeisimmän soveltuvan hissivec-vektorin keskiarvo
    keskidata[datalaskuri] = hissivec;
    keskiarvo = ((keskidata[0] + keskidata[1] + keskidata[2] + keskidata[3] + keskidata[4]
                  + keskidata[5] + keskidata[6] + keskidata[7] + keskidata[8] + keskidata[9]) / 10);
                  
    datalaskuri++;
    if (datalaskuri >= 10){
        datalaskuri = 0;
    }
    
    if (keskiarvo >= 1.05) {
        hissilaskuri++;
        hissivirhe = 1;
    } else if (hissivirhe >= 1) {
            hissivirhe = 0;
    } else {
        hissilaskuri = 0;
    }
    
    //Jos päästiin saatiin kolme soveltuvaa keskiarvoa, tunnistetaan hissimatka ja tyhjennetään keskidata-taulukko
    if (hissilaskuri >= 3) {
        hissit++;
        hissilaskuri = 0;
        tyhjennakeskidata();
    } 
}

void tyhjennakeskidata(void) {
    keskidata[0] = 0.0;
    keskidata[1] = 0.0;
    keskidata[2] = 0.0;
    keskidata[3] = 0.0;
    keskidata[4] = 0.0;
    keskidata[5] = 0.0;
    keskidata[6] = 0.0;
    keskidata[7] = 0.0;
    keskidata[8] = 0.0;
    keskidata[9] = 0.0;
}

/* JTKJ: Handle for power button */
Void powerButtonFxn(PIN_Handle handle, PIN_Id pinId) {
    
    
if (valikkoruutu == 1) {
        
    //Laite pois päältä
    Display_clear(hDisplay);
    Display_close(hDisplay);
    Task_sleep(100000 / Clock_tickPeriod);

	PIN_close(hPowerButton);

    PINCC26XX_setWakeup(cPowerWake);
	Power_shutdown(NULL,0);
       
} else if (valikkoruutu == 2) {
    switch (valinta) {
        case 1:
            askeleet = 0;
            portaat = 0;
            hissit = 0;
            break;
        case 2:
            
            sprintf(sendload,"%d askelta", askeleet); 
            Send6LoWPAN(IEEE80154_SERVER_ADDR, sendload, strlen(sendload));
            break;
        case 3:
            valikkoruutu = 1;
            break;
   }
}
    
}

/* JTKJ: Button0 käsittelijäfunktio */
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

   // Vaihdetaan led-pinnin tilaa negaatiolla
   PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
   
   //Vaihdetaan 
   if (valikkoruutu == 1) {
       valikkoruutu = 2;
   } else if (valikkoruutu == 2) {
       valinta++;
       if (valinta >= 4) {
           valinta = 1;
       }
   }
   
   
   
   /*
   char payload[16]; // viestipuskuri
   sprintf(payload,"%d askelta", askeleet); 
   Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload)); */

}

/* JTKJ: Communication Task */
    Void commTask(UArg arg0, UArg arg1) {
        
    uint16_t senderAddr;

    // Aina lähetyksen jälkeen pitää palata vastaanottotilaan
    StartReceive6LoWPAN();
   
   // HUOM! VIESTEJÄ EI SAA LÄHETTÄÄ SILMUKASSA
   // Silmukassa viestejä lähtee niin usein, että se tukkii kanavan 
   // kaikilta muilta samassa harjoituksissa olevilta!!

    
    while (1) {

        // DO __NOT__ PUT YOUR SEND MESSAGE FUNCTION CALL HERE!! 

    	// NOTE THAT COMMUNICATION WHILE LOOP DOES NOT NEED Task_sleep
    	// It has lower priority than main loop (set by course staff)
    	if (GetRXFlag()) {

            // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
            memset(payload,0,16);
            // Luetaan viesti puskuriin payload
            Receive6LoWPAN(&senderAddr, payload, 16);
            // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
            System_printf(payload);
            System_flush(); 
    	}
    	
    	
    	Task_sleep(1000 / Clock_tickPeriod);
        
    }
}

/* sensor Task*/
Void sensorTask(UArg arg0, UArg arg1) {

    // *******************************
    // USE TWO DIFFERENT I2C INTERFACES
    // *******************************
	I2C_Handle i2c; // INTERFACE FOR OTHER SENSORS
	I2C_Params i2cParams;
	I2C_Handle i2cMPU; // INTERFACE FOR MPU9250 SENSOR
	I2C_Params i2cMPUParams;
    
	float ax, ay, az, gx, gy, gz;
	double pres,temp;
	char str[80];

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // *******************************
    // MPU OPEN I2C
    // *******************************
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // *******************************
    // MPU POWER ON
    // *******************************
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // WAIT 100MS FOR THE SENSOR TO POWER UP
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // *******************************
    // MPU9250 SETUP AND CALIBRATION
    // *******************************
	System_printf("MPU9250: Setup and calibration...\n");
	System_flush();

	mpu9250_setup(&i2cMPU);

	System_printf("MPU9250: Setup and calibration OK\n");
	System_flush();

    // *******************************
    // MPU CLOSE I2C
    // *******************************
    I2C_close(i2cMPU);

    // *******************************
    // OTHER SENSOR OPEN I2C
    // *******************************
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    // BMP280 SETUP
    bmp280_setup(&i2c);

    // *******************************
    // OTHER SENSOR CLOSE I2C
    // *******************************
    I2C_close(i2c);

    // LOOP FOREVER
	//for (i = 0; i < 5; i++) {
	
/* sensori looppi alkaa */

	while (1) {

	    // *******************************
	    // OTHER SENSORS OPEN I2C
	    // *******************************
	    i2c = I2C_open(Board_I2C, &i2cParams);
	    if (i2c == NULL) {
	        System_abort("Error Initializing I2C\n");
	    }

	    // *******************************
	    // BMP280 ASK DATA
	    // *******************************
	    bmp280_get_data(&i2c, &pres, &temp);

    	//sprintf(str,"%f %f\n",pres,temp);
    	//System_printf(str);
    	//System_flush();

	    // *******************************
	    // OTHER SENSORS CLOSE I2C
	    // *******************************
	    I2C_close(i2c);

	    // *******************************
	    // MPU OPEN I2C
	    // *******************************
	    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
	    if (i2cMPU == NULL) {
	        System_abort("Error Initializing I2CMPU\n");
	    }

	    // *******************************
	    // MPU ASK DATA
        //    Accelerometer values: ax,ay,az
	 	//    Gyroscope values: gx,gy,gz
	    // *******************************
		mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
		
		
	    // MPU CLOSE I2C
	    I2C_close(i2cMPU);
        
        //Kutsutaan sensorien dataa tulkitsevaa algoritmia
        sensorialgoritmi(ax, ay, az);


        
        
	    // WAIT 200MS
    	Task_sleep(200000 / Clock_tickPeriod);
	}

	// MPU9250 POWER OFF 
	//     Because of loop forever, code never goes here
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}


/* JTKJ: Screen update task */
Void labTask(UArg arg0, UArg arg1) {

    /* JTKJ: Init Display */
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL) {
        System_abort("Error initializing Display\n");
    }

    /* JTKJ: Check that Display works */
    Display_clear(hDisplay);

    /* JTKJ: näytönpiirtosilmukka */
    while (1) {
    	
    	char piirto1[50]; //Askeleiden piirtotaulukko
    	char piirto2[50]; //Portaiden piirtotaulukko
        char piirto3[50]; //Hissien piirtotaulukko
        
        
        
        if (valikkoruutu == 1) {
            
            Display_print0(hDisplay, 0, 1, "Asetukset");
            Display_print0(hDisplay, 11, 1, "Sulje laite");
            
            sprintf(piirto1,"Askeleet: %d", askeleet);
            sprintf(piirto2,"Portaat: %d", portaat);
            sprintf(piirto3,"Hissit: %d", hissit);
            Display_print0(hDisplay, 3, 1, piirto1);
            Display_print0(hDisplay, 4, 1, piirto2 );
            Display_print0(hDisplay, 5, 1, piirto3);
        
            if (strlen(payload) >= 1) {
                Display_print0(hDisplay, 8, 1, "uusi viesti:");
                Display_print0(hDisplay, 9, 1, payload);
            }
        
        
        } else if (valikkoruutu == 2) {
            
            Display_print0(hDisplay, 0, 1, "Vaihda valintaa");
            Display_print0(hDisplay, 11, 1, "Valitse");
        
            switch (valinta) {
                case 1:
                    Display_print0(hDisplay, 3, 1, "Nollaa <");
                    Display_print0(hDisplay, 4, 1, "Leuhka viesti");
                    Display_print0(hDisplay, 5, 1, "Palaa");
                    break;
                case 2:
                    Display_print0(hDisplay, 3, 1, "Nollaa");
                    Display_print0(hDisplay, 4, 1, "Leuhka viesti <");
                    Display_print0(hDisplay, 5, 1, "Palaa");
                    break;
                case 3:
                    Display_print0(hDisplay, 3, 1, "Nollaa");
                    Display_print0(hDisplay, 4, 1, "Leuhka viesti");
                    Display_print0(hDisplay, 5, 1, "Palaa <");
                    break;
        }
            
        }
    	// JTKJ: Do not remove sleep-call from here!
    	Task_sleep(1000000 / Clock_tickPeriod);
        
    }
}



/* Pääfunktio */
Int main(void) {
    
    System_printf("Hello world! Tehtävä 2 toimii?\n");
    System_flush();
    
    // Task variables
	Task_Handle hLabTask;
	Task_Params labTaskParams;
	Task_Handle hCommTask;
	Task_Params commTaskParams;
	Task_Handle hSensorTask;
	Task_Params sensorTaskParams;
	
    // Initialize board
    Board_initGeneral();
    Board_initI2C();

	/* JTKJ: Power Button */
	hPowerButton = PIN_open(&sPowerButton, cPowerButton);
	if(!hPowerButton) {
		System_abort("Error initializing power button shut pins\n");
	}
	if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0) {
		System_abort("Error registering power button callback function");
	}

    /* JTKJ: Button 0 */
    hButton0 = PIN_open(&sButton0, cButton0);
    if(!hButton0) {
       System_abort("Error initializing button pins\n");
    }
    if (PIN_registerIntCb(hButton0, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }
   
    /* Mpu Pinni juttuja */
    hLed = PIN_open(&sLed, cLed);
    if(!hLed) {
        System_abort("Error initializing LED pins\n");
    }
   
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
    	System_abort("Pin open failed!");
    }
   
   
    /* JTKJ: Init sensor Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=1;
    hSensorTask = Task_create(sensorTask, &sensorTaskParams, NULL);
    if (hSensorTask == NULL) {
        System_abort("Task create failed!");
    }
    

    /* JTKJ: Init Main Task */
    Task_Params_init(&labTaskParams);
    labTaskParams.stackSize = STACKSIZE;
    labTaskParams.stack = &labTaskStack;
    labTaskParams.priority=2;
    hLabTask = Task_create(labTask, &labTaskParams, NULL);
    if (hLabTask == NULL) {
    	System_abort("Task create failed!");
    }
    

    /* JTKJ: Init Communication Task */
    Init6LoWPAN();

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=3;
    hCommTask = Task_create(commTask, &commTaskParams, NULL);
    if (hCommTask == NULL) {
    	System_abort("Task create failed!");
    }
    

    /* Start BIOS */
    BIOS_start();

    return (0);
}

