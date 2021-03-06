/*
**
**                           Main.c
**
**
**********************************************************************/
/*
   Last committed:     $Revision: 00 $
   Last changed by:    Brandon Piner
   Last changed date:  21-09-2015

**********************************************************************/

/**INCLUDES*/
#include "stm32f0xx_conf.h"
#include <stdio.h>
#include <string.h>
#include "umqtt.h"
#include "config.h"
#include "Debug.h"
#include "SIM808.h"
#include "timing.h"
#include "servo.h"
#include "bmp180.h"
#include "HCSR04.h"


/**DEFINES*/
#define LINEMAX 50
#define umqtt_build_header(type, dup, qos, retain) \
	(((type) << 4) | ((dup) << 3) | ((qos) << 1) | (retain))
#define MAINVERBOSE 1

/**PUBLIC VARIABLES*/\
char *connackAccept = {0x20, 0x02, 0,0};
char *subscribeAccept={0x90, 0x03};
extern uint8_t msgTimout;
uint8_t mqtt_txbuff[200];
uint8_t mqtt_rxbuff[150];

struct umqtt_connection mqtt = {
	.txbuff = {
		.start = mqtt_txbuff,
		.length = sizeof(mqtt_txbuff),
	},
	.rxbuff = {
		.start = mqtt_rxbuff,
		.length = sizeof(mqtt_rxbuff),
	},
//	.message_callback = handle_message,
};


char rxData [300];
extern char rxBuf[300];
extern uint16_t rxBufLen;
extern char receivedDebug[200];
extern uint16_t receivedDebugLen;

static struct bmp180_t Sensor1, Sensor2;
static HCSR04_t UltrasonicSensor;
//set to off if you dont want to activate the sim808
tcp_state current_state = STATE_OFF;
static struct sim808_t sim808;

/**PRIVATE FUNCTIONS*/
void receiveUARTCommand();


/**MAIN PROGRAM*/
int main(void)
{
    /**PRIVATE VARIABLES*/
    uint8_t flagNetReg=0, flagAlive=0, strtosend[40];
    uint16_t index=0;
    double lng, lat;
    uint8_t data1, data2, waterLevel = 0;
    static int32_t pressure1=0, pressure2 =0;
    char latCoord[20], longCoord[20];
    static uint8_t loopCounter=0;
/** Initialise functions
***************************
*   initTiming
*   initGPIO
*   initDebug
*   initSim
**/
    initDelay();
    gpioInit();
    debugInit();
    simInit();
    I2CInit();
    servoInit();

/**START THE SIM808*/
    debugSend("\n----begin----\n");
    simSend("AT+CPOWD=1");
    delayMilliIT(500);
    GPIO_ResetBits(GPIOA, GPIO_Pin_5);
    delayMilliIT(2500);
    GPIO_SetBits(GPIOA, GPIO_Pin_5);
    debugSend("\n----SIM On----\n");
    delayMilliIT(6000);

    simNoEcho();
    delayMilliIT(20);

/**START THE GPS*/
    initGPS();///only AFTER SIM808 has turned on

/**GET CONFIG DATA FROM PRESSURE SENSORS*/
    bmp180_get_calib_param(I2C1, &Sensor1);
    bmp180_get_calib_param(I2C2, &Sensor2);
  /*  while(1)
    {
        delayMilliIT(20);
        resetWatchdog();
        loopCounter++;
        _printfU("\nLoop Counter: ", loopCounter);
        if(loopCounter>=200) loopCounter=0;
        ///PING "AT"
        if(simPing()==0)
        {///IF NO PING RESPONSE THEN RESET
            debugSend("\nping-NO-response");
            NVIC_SystemReset();
        }

        simUpdateState(&current_state);

        if(current_state!=STATE_CONNECTED)
        {
            debugSend("\nconnection no longer alive");
            //NVIC_SystemReset();
        }
        ///Receive any subscribed packets
        //recievePacket();
        receiveUARTCommand();


        ///Read Water Level
        waterLevel = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6);
        _printfU("Water Level ", waterLevel);


        ///Get Atmospheric Pressure
        pressure1 = pressureAverage(I2C1, &Sensor1);
        //_printfLngS("\nAtmospheric Pressure is ", (int32_t)pressure1);

        ///Get GPS Coords
        simGPSInfo(&sim808);
        simGPSStatus(&sim808);
        ///Get Ultrasonic Distance

        servo_DeInit();
        HCSR04_Init();
        HCSR04_Read(&UltrasonicSensor);
        delayMilli(2);

        servoInit();
        //_printfLngU("Ultrasonic Distance: ", UltrasonicSensor.Distance);

    ///MQTT Tranmit packet

        debugSend("\n-transmit1-");
        mqtt.txbuff.pointer= mqtt.txbuff.start;
        mqtt.txbuff.datalen=0;
        sprintf(strtosend, "{\"lng\":%s,\"lat\":%s,\"fix\":\"%sx\",\"numSat\":%s,\"time\":%s}", sim808.longitudeCoord, sim808.latitudeCoord, sim808.fixStatus, "0", sim808.time);
        umqtt_publish(&mqtt, "test/gps", strtosend, strlen(strtosend));
        debugSend2(strtosend,strlen(strtosend));
        //simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
        debugSend("\n-transmit2-");
        mqtt.txbuff.pointer= mqtt.txbuff.start;
        mqtt.txbuff.datalen=0;
        sprintf(strtosend, "{\"pressure1\":%d,\"pressure2\":%d,\"ultrasonic\":%d}", pressure1, pressure2, UltrasonicSensor.Distance);
        umqtt_publish(&mqtt, "test/sensor", strtosend, strlen(strtosend));
        debugSend2(strtosend,strlen(strtosend));
        //simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);


        if(loopCounter%10==0)
        {
            ///Get Balloon Pressure
            pressure2 = pressureAverage(I2C2, &Sensor2);
            _printfLngS("\nBalloon Pressure is ", (int32_t)pressure2);
            ///Get Battery Percentage
            simBatteryCheck(&sim808);
            debugSend("Batt check done: ");
            debugSend2(sim808.batteryPercentage, 2);
            debugSend("\n");

            debugSend("\n-transmit3-");
            mqtt.txbuff.pointer= mqtt.txbuff.start;
            mqtt.txbuff.datalen=0;
            sprintf(strtosend, "{\"bat\":%s,\"chrg\":%s}", sim808.batteryPercentage, sim808.charge);
            umqtt_publish(&mqtt, "test/data", strtosend, strlen(strtosend));
            debugSend2(strtosend,strlen(strtosend));
            //simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
        }


        if(loopCounter%15==0)
            servoNone();

    }
    NVIC_SystemReset();*/


/** \name Check SIM808 */
    checkInitalStatus(&current_state);

    ///check what state the TCP is in.
    whatStateAmIIn(&current_state);
    if(current_state!= STATE_CONNECTED)
        simConnect();
    ///Authenticate with the MQTT broker
    nethandler_umqtt_init(&mqtt);
    simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
    recievePacket();
    if(strstr(rxBuf, connackAccept) != NULL)
        debugSend("\nMQTT Connection accepted");
    else
    {
        debugSend("\nMQTT Connection refused");
        NVIC_SystemReset();
    }



    ///Subscribe to "test/action" topic
    mqtt.txbuff.pointer= mqtt.txbuff.start;
    mqtt.txbuff.datalen=0;
    umqtt_subscribe(&mqtt, "test/action");
    simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
    recievePacket();


    while(1)
    {
        delayMilliIT(20);
        resetWatchdog();
        loopCounter++;
        _printfU("\nLoop Counter: ", loopCounter);
        if(loopCounter>=200) loopCounter=0;
        ///PING "AT"
        if(simPing()==0)
        {///IF NO PING RESPONSE THEN RESET
            debugSend("\nping-NO-response");
            NVIC_SystemReset();
        }

        simUpdateState(&current_state);

        if(current_state!=STATE_CONNECTED)
        {
            debugSend("\nconnection no longer alive");
            NVIC_SystemReset();
        }
        ///Receive any subscribed packets
        recievePacket();
        //receiveUARTCommand();


        ///Read Water Level
        waterLevel = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6);
        _printfU("Water Level ", waterLevel);


        ///Get Atmospheric Pressure
        pressure1 = pressureAverage(I2C1, &Sensor1);
        //_printfLngS("\nAtmospheric Pressure is ", (int32_t)pressure1);

        ///Get GPS Coords
        simGPSInfo(&sim808);
        simGPSStatus(&sim808);
        ///Get Ultrasonic Distance
        /**Must deinitalise the Servo because it uses TIM2 also.*/
        servo_DeInit();
        HCSR04_Init();
        HCSR04_Read(&UltrasonicSensor);
        delayMilli(2);
        /**Re-activate the servo TIM2*/
        servoInit();
        //_printfLngU("Ultrasonic Distance: ", UltrasonicSensor.Distance);

    ///MQTT Tranmit packet

        debugSend("\n-transmit1-");
        mqtt.txbuff.pointer= mqtt.txbuff.start;
        mqtt.txbuff.datalen=0;
        sprintf(strtosend, "{\"lng\":%s,\"lat\":%s,\"fix\":\"%sx\",\"numSat\":%s,\"time\":%s}", sim808.longitudeCoord, sim808.latitudeCoord, sim808.fixStatus, "0", sim808.time);
        umqtt_publish(&mqtt, "test/gps", strtosend, strlen(strtosend));
        debugSend2(strtosend,strlen(strtosend));
        simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
        debugSend("\n-transmit2-");
        mqtt.txbuff.pointer= mqtt.txbuff.start;
        mqtt.txbuff.datalen=0;
        sprintf(strtosend, "{\"pressure1\":%d,\"pressure2\":%d,\"ultrasonic\":%d}", pressure1, pressure2, UltrasonicSensor.Distance);
        umqtt_publish(&mqtt, "test/sensor", strtosend, strlen(strtosend));
        debugSend2(strtosend,strlen(strtosend));
        simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);


        /**THIS DATA DOESNT NEED TO BE SENT AS OFTEN*/
        if(loopCounter%10==0)
        {
            ///Get Balloon Pressure
            pressure2 = pressureAverage(I2C2, &Sensor2);
            _printfLngS("\nBalloon Pressure is ", (int32_t)pressure2);
            ///Get Battery Percentage
            simBatteryCheck(&sim808);
            debugSend("Batt check done: ");
            debugSend2(sim808.batteryPercentage, 2);
            debugSend("\n");

            debugSend("\n-transmit3-");
            mqtt.txbuff.pointer= mqtt.txbuff.start;
            mqtt.txbuff.datalen=0;
            sprintf(strtosend, "{\"bat\":%s,\"chrg\":%s}", sim808.batteryPercentage, sim808.charge);
            umqtt_publish(&mqtt, "test/data", strtosend, strlen(strtosend));
            debugSend2(strtosend,strlen(strtosend));
            simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
        }

        /**Set servo to neutral */
        if(loopCounter%15==0)
            servoNone();
    }
}

void gpioInit()
{
    /**
     * PA0 - Ultrasonic Trigger
     * PA1 - Ultrasonic Echo
     * PA2 - TX2 - to SIM RX
     * PA3 - RX2 - to SIM TX
     * PA4 -
     * PA5 - SIM Power
     * PA6 - Water Level
     * PA7 - Test pad
     * PA8 -
     * PA9  - TX1 - debug
     * PA10 - RX1 - debug
     * PA15 - PWM - servo
     * PB0 - Test pad
     * PB1 - Test pad
     * PB2 - Test pad
     */
    ///PORT A
    //USART 1 and 2
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
    //GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_1);//CTS2
   // GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_1);//RTS2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);//TX2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);//RX2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_1); // TIM3_CH1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_1);//TX1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_1);//RX1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource15, GPIO_AF_2);//TIM2_CH1_Servo
    GPIO_InitTypeDef GPIO_InitStruct;
        GPIO_InitStruct.GPIO_Pin =GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_6|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_15;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
        GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
        GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin =GPIO_Pin_5|GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin =GPIO_Pin_6;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    ///PORT B
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_1);
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    //Make sure GPIOInit is called before this funciton is called
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    I2C_InitTypeDef I2C_InitStruct;
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_Timing = 0x0010020B;
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStruct.I2C_AnalogFilter = I2C_AnalogFilter_Enable;
    I2C_InitStruct.I2C_DigitalFilter=0;
    I2C_Init(I2C1, &I2C_InitStruct);

    I2C_Cmd(I2C1, ENABLE);

}



void recievePacket(void)
{
    uint8_t rxPacketLen=1, i, commaCount=0, commaPos[10], temp[2], mqttPacket[100];
    ///Receive TCP RX
    flushReceiveBuffer();
    simSend("AT+CIPRXGET=2,100");
    while(simAvailable()==0);


    if(rxBuf[16]==0x2C)
    {
        //One digit length
        rxPacketLen = (rxBuf[15])&0x0F;
    }
    else if (rxBuf[17]==0x2C)
    {
        //Two digits long
        rxPacketLen = (rxBuf[16])&0x0F;//ones position
        rxPacketLen += 10*((rxBuf[15])&0x0F);// tens position
    }
    else
    {
        temp[0]=0;
        temp[1]=0;
        rxPacketLen=0;
    }

    memcpy(&mqttPacket[0], &rxBuf[21], rxPacketLen);
    if(rxPacketLen>0)
    {
        debugSend("Packet: ");
        debugSend2(mqttPacket,rxPacketLen);
        USART_SendData(USART1, rxPacketLen);
        debugSend("\n");
        if(mqttPacket[1]==0X11)
        {
            debugSend("- - - going places\n");
        }
        if(mqttPacket[18]=='X')
        {
            servoUp();
            debugSend("- - - going up\n");
        }
        if(mqttPacket[18]=='N')
        {
            debugSend("- - - going down");
            servoDown();

        }
        if(mqttPacket[18]=='P')
        {
            servoDrop();
            debugSend("- - - going drop");
        }
        if(mqttPacket[18]=='E')
        {
            servoNone();
            debugSend("- - - neutral position\n");
        }
        if(mqttPacket[18]=='Q')
        {
            debugSend("- - - QUIT OPERATIONS and DISCONNECT\n");
            mqtt.txbuff.pointer= mqtt.txbuff.start;
            mqtt.txbuff.datalen=0;
            umqtt_disconnect(&mqtt);
            simTransmit(mqtt_txbuff,mqtt.txbuff.datalen);
            while(1)
            {
                simSend("AT+CPOWD=1");
                debugSend("- - - QUIT OPERATIONS and DISCONNECT\n");
                delayMilliIT(2000);
            }

        }
    }

}


void receiveUARTCommand()
{
    uint8_t rxPacketLen=1, i, commaCount=0, commaPos[10], temp[2], mqttPacket[100];
    ///Receive UART RX
    if(debugReceive()==1)
    {
        debugSend("Packet: ");
        debugSend(receivedDebug);
        debugSend("\n");
        if(receivedDebug[0]=='U')
        {
            servoUp();
            debugSend("- - - going up\n");
        }
        if(receivedDebug[0]=='D')
        {
            debugSend("- - - going down");
            servoDown();

        }
        if(receivedDebug[0]=='R')
        {
            servoDrop();
            debugSend("- - - going drop");
        }
        if(receivedDebug[0]=='N')
        {
            servoNone();
            debugSend("- - - neutral position\n");
        }
        if(receivedDebug[0]=='Q')
        {
            debugSend("- - - QUIT OPERATIONS and DISCONNECT\n");
            while(1)
            {
                debugSend("- - - QUIT OPERATIONS and DISCONNECT\n");
                delayMilliIT(2000);
            }
        }
    }
    debugFlushRx();
}

void parseData(char *payload, char *latitude , char *longitude, int32_t altitude, uint8_t battery)
{

    sprintf(payload, "{\"lat\":%s,\"lng\":%s, \"alt\":%d, \"bat\":%d}", latitude, longitude, altitude, battery);
}

void parseGPS(char * CBCstring, char * battery)
{
    char charge[10], voltage[5];

    sscanf(CBCstring, "%[^,],%[^,],%[^,]", &charge, &battery, &CBCstring);

}
