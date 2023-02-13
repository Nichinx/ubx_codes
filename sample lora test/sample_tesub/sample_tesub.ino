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
volatile bool done = false;

void setup() {
    Serial.begin(115200);
    while(!Serial);
    
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
//    start = millis();

//    while(now.second() != 0){
//      delay(10);
//    }

}

//void loop(){
////  if (samplingTime() && samplingSec()){
////        read_data();
////        send_thru_lora(dataToSend);
////////        start = millis();
//////        done = true;      
////  }
////  Serial.println("print"); 
//////  if ((millis() - start) >= 58000){
//////       done = false;
//////       start = millis();
//////  }
//////  delay(1000);
////}
//
//if (millis() - start <= 60000){ //every minute interval
//  Serial.println("getting rtcm");
//  delay(250);
//} else {
//  read_data();
//  send_thru_lora(dataToSend);
//  start = millis();
//}
//}

void loop(){
  if (samplingTime() && samplingSec()){
    read_data();
    send_thru_lora(dataToSend);
  }
  Serial.println("print");
  delay(1000);
}
