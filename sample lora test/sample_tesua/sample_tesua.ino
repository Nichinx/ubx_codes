#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <SPI.h>
#include <RH_RF95.h>
#include <LowPower.h>
#include <EnableInterrupt.h>
#include <FlashStorage.h>
#include "Sodaq_DS3231.h"
#include <Adafruit_SleepyDog.h>

#define BUFLEN (5*RH_RF95_MAX_MESSAGE_LEN) //max size of data burst we can handle - (5 full RF buffers) - just arbitrarily large
#define RFWAITTIME 500 //maximum milliseconds to wait for next LoRa packet - used to be 600 - may have been too long

char dataToSend[200];
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

// LED is turned on at 1st LoRa reception and off when nothing else received. It gives an indication of how long the incoming data stream is.
#define LED 13
unsigned long start;

void setup() {
    Serial.begin(115200);
//    while(!Serial);
    
    rtc.begin();
//    DateTime now = rtc.now();
    
    pinMode(LED, OUTPUT);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

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
    readTimeStamp();
    Serial.print("Current timestamp: ");
    Serial.println(Ctimestamp);

//    while(now.second() != 0){
//      Serial.println(".");
//      delay(1);
//    }
}

//02.01.23 sample
void loop(){
//  start = millis(); 
 
//  do {
//    readTimeStamp();
//    Serial.print("Current timestamp: ");
//    Serial.println(Ctimestamp);
//  } while ((millis() - start) < 180000);

    if (samplingTime() && samplingSec()){
//  if (samplingMin() && samplingSec()){
//    Serial.print("sampling time . . . ");
    read_data();
    send_thru_lora(dataToSend);
  } //else {
    Serial.println("print");
    delay(1000);
//  }
   
//  attachInterrupt(RTCINTPIN, wake, FALLING);
//  setAlarmEvery30(7);
//  rtc.clearINTStatus();
//  attachInterrupt(RTCINTPIN, wake, FALLING);
}

//void loop() {
//
//    if (millis() - start <= 60000){ //every minute interval
//      Serial.println("getting rtcm");
//      delay(250);
//    } else {   
//      readTimeStamp();
//      Serial.print("Current timestamp: "); 
//      Serial.println(Ctimestamp);
//      
//      read_data();
//      send_thru_lora(dataToSend);
//  
//      start = millis();
//    }
//}
