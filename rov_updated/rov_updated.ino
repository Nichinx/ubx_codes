#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <SPI.h>
#include <RH_RF95.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <LowPower.h>
#include <EnableInterrupt.h>
#include <FlashStorage.h>
#include "Sodaq_DS3231.h"
#include <Adafruit_SleepyDog.h>
SFE_UBLOX_GNSS myGNSS;

#define BUFLEN (5*RH_RF95_MAX_MESSAGE_LEN) //max size of data burst we can handle - (5 full RF buffers) - just arbitrarily large
#define RFWAITTIME 500 //maximum milliseconds to wait for next LoRa packet - used to be 600 - may have been too long

char dataToSend[200];
//#define DATA_TO_AVERAGE 1 //changed from 5 to 1 -- removes averaging

char Ctimestamp[13] = "";
uint16_t store_rtc = 00; //store rtc alarm
volatile bool OperationFlag = false;
#define DEBUG 1
#define RTCINTPIN 6
#define VBATPIN A7    //new copy
#define VBATEXT A5

// for feather m0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#define RF95_FREQ 433.0
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// We will use Serial2 - Rx on pin 11, Tx on pin 10
Uart Serial2 (&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);

void SERCOM1_Handler() {
  Serial2.IrqHandler();
}

// LED is turned on at 1st LoRa reception and off when nothing else received. It gives an indication of how long the incoming data stream is.
#define LED 13
unsigned long start;

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200);

    // Assign pins 10 & 11 SERCOM functionality
    pinPeripheral(10, PIO_SERCOM);
    pinPeripheral(11, PIO_SERCOM);
    delay(100);
    
    rtc.begin();
    attachInterrupt(RTCINTPIN, wake, FALLING);
//    init_Sleep(); //initialize MCU sleep state      --v.2: will not let MCU sleep.
    setAlarmEvery30(7); //rtc alarm settings


    pinMode(LED, OUTPUT);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

//    digitalWrite(RFM95_RST, LOW);
//    delay_millis(10);
//    digitalWrite(RFM95_RST, HIGH);
//    delay_millis(10);

    Serial.println("Feather LoRa RX");
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    for (int y = 0; y < 20; y++){
      if (!rf95.init()){
        Serial.println("LoRa radio init failed");
        delay(1000); 
      } else {
        Serial.println("LoRa radio init OK!");
        break;
      }
    }

    for (int z = 0; z < 20; z++){
      if (!rf95.setFrequency(RF95_FREQ)) {
        Serial.println("setFrequency failed");
        delay(1000);
      } else {
        Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
        break;
      }
    }
    
    rf95.setTxPower(23, false);
    init_ublox();
    readTimeStamp();
    Serial.print("Current timestamp: ");
    Serial.println(Ctimestamp);
}

//void loop() {
//  start = millis(); 
//
//   do {
//      get_rtcm();
//   } while ((RTK() != 2) && ((millis() - start) < 300000));
//
//   if (RTK() == 2 && SIV() >= 30){   //added SIV count
//       for (int c = 0; c < 10; c++){    //added counter
//          read_ublox_data();
//          send_thru_lora(dataToSend);
//       }
//   } 
//    
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  setAlarmEvery30(4);
//  rtc.clearINTStatus();
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  sleepNow();
//}

//using samplingTime() 
//void loop(){
//  start = millis();
//
//  if (!samplingTime()) {
//    get_rtcm();
//  }
//  
//  else {
//    do {
//      get_rtcm();
//    } while ((RTK() != 2) && ((millis() - start) < 300000)); 
//    
//    if (RTK() == 2 && SIV() > 30) {   //comment out muna sa SIV, since unsure pa sa value
////    if (RTK() == 2) {
//      read_ublox_data();
//      send_thru_lora(dataToSend);
//    }      
//  }
//
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  samplingTime();
//  rtc.clearINTStatus();
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//}


//if ayaw magwork ng samplingTime()
//void loop(){
//  start = millis(); 
//  
//  do {
//     get_rtcm();
//  } while ((RTK() != 2) && ((millis() - start) < 180000)); //from 300000 (5mins) to 180000 (3mins)
//  
//  if (RTK() == 2 && SIV() >= 30){
//    read_ublox_data();
//    send_thru_lora(dataToSend);
//  } 
//
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  setAlarmEvery30(4);
//  rtc.clearINTStatus();
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//}

//v3
//void loop(){
//  start = millis(); 
//  
//  do{
//    get_rtcm();
//  } while ((RTK() != 2) && ((millis() - start) < 180000)); //from 300000 (5mins) to 180000 (3mins)
//  
//  if (RTK() == 2 && SIV() >= 30) {   //comment out muna sa SIV, since unsure pa sa value
//    read_ublox_data();
//    send_thru_lora(dataToSend);
//  }    
//    
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  setAlarmEvery30(4);
//  rtc.clearINTStatus();
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//}

//v4 01.30.2023
//void loop(){
//  start = millis(); 
// 
//  do {
//    get_rtcm();
//  } while (RTK() != 2);
//  
//  if (RTK() == 2 && SIV() >= 30) {   //comment out muna sa SIV, since unsure pa sa value
//      if (!samplingTime()){
//        get_rtcm();
//      }
//      else {
//        read_ublox_data();
//        send_thru_lora(dataToSend);
//        break;
//      } 
//    }
//  }
//      
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  setAlarmEvery30(4);
//  rtc.clearINTStatus();
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//}

//v5 01.31.2023
void loop(){
//  start = millis(); 
 
//  do {
    get_rtcm();
//  } while (RTK() != 2);
  
  if (RTK() == 2 && SIV() >= 30) {  
    if (samplingTime() && samplingSec()){
      read_ublox_data();
      send_thru_lora(dataToSend);
    } 
  }
}
