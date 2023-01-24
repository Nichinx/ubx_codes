
/**
 * Champagne Datalogger
 * Surficial Tilt IMU sensor added

Features:
* Sends sensor data via LoRa or GSM
* Built-in rtc with configurable wake interrupt
* Low power mode ~10uA
* ~0.5mA with IMU sensor

The circuit:
* Champagne Board
* Adafruit Feather M0
* RFM95 433MHz
* Real time clock
* GSM

Created: January 2020
By : MAD, TEP
Modified: February 21, 2022
*/

#include <Wire.h>
#include <LowPower.h>
#include "Sodaq_DS3231.h"
#include <SPI.h>
#include <RH_RF95.h>
#include <avr/dtostrf.h> // dtostrf missing in Arduino Zero/Due
#include <EnableInterrupt.h>
#include <FlashStorage.h>
#include <Arduino.h>        // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <string.h>
#include <Adafruit_SleepyDog.h>

#define BAUDRATE 115200
#define DUEBAUD 9600
#define DUESerial Serial1
#define RTCINTPIN 6
#define DUETRIG 5
#define DEBUG 1
#define VBATPIN A7
#define VBATEXT A5
#define GSMRST 12
#define GSMPWR A2
#define GSMDTR A1
#define GSMINT A0   // gsm ring interrupt
#define IMU_POWER 9 // A3-17

// gsm related
#define GSMBAUDRATE 9600
#define GSMSerial Serial2
#define MAXSMS 168

// for m0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 433.0          // Change to 434.0 or other frequency, must match RX's freq!
#define DATALEN 200              // max size of dummy length
// #define LORATIMEOUT 500000       // 8.33 minutes delay
#define LORATIMEOUT 600000        // 10 minutes delay
#define LORATIMEOUTMODE2 900000  // 15.0 mnutes
#define LORATIMEOUTMODE3 1200000 // 20.0 mnutes
#define DUETIMEOUT 210000        // 3.50 minutes timeout
#define DUEDELAY 60000           // 1.0 minute delay
#define RAININT A4               // rainfall interrupt pin A4
#define DEBUGTIMEOUT 300000      // debug timeout in case no data recieved; 60K~1minute

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// initialize LoRa global variables
char dataToSend[DATALEN]; // lora
uint8_t payload[RH_RF95_MAX_MESSAGE_LEN];
uint8_t len = sizeof(payload);

// LoRa received
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t len2 = sizeof(buf);
char received[260];

char streamBuffer[250]; // store message
char rssiString[250];
char voltMessage[200];
int customDueFlag = 0; // for data gathering
int sendToLoRa = 0;
String tx_RSSI;   // tx rssi of sensor A
String tx_RSSI_B; // tx rssi of sensor B
String tx_RSSI_C; // tx rssi of sensor C
bool valid_LoRa_tx = false;

// rain gauge
static unsigned long last_interrupt_time = 0;
const unsigned int DEBOUNCE_TIME = 40; // 40
volatile float rainTips = 0.00;
char sendRainTip[7] = "0.00";

volatile bool gsmRingFlag = false;  // gsm interrupt
volatile bool rainFallFlag = false; // rain tips
volatile bool OperationFlag = false;
bool getSensorDataFlag = false;
bool debug_flag_exit = false;
bool send_rain_data_flag = true;

char firmwareVersion[9] = "22.02.17"; // year . month . date
char station_name[6] = "MADTA";
char Ctimestamp[13] = "";
char command[30];
char txVoltage[100] = "0";
char txVoltageB[100] = "0";
char txVoltageC[100] = "0";

unsigned long timestart = 0;
uint8_t serial_flag = 0;
uint8_t debug_flag = 0;
uint8_t rcv_LoRa_flag = 0;
uint16_t store_rtc = 00; // store rtc alarm
// uint8_t gsm_power = 0; //gsm power (sleep or hardware ON/OFF)

// GSM
String serverNumber = ("639175972526");
bool gsmPwrStat = true;
String tempServer, regServer;
char _csq[10];
char response[150];
bool registerNumber = false;

/* Pin 11-Rx ; 10-Tx (GSM comms) */
Uart Serial2(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);
void SERCOM1_Handler()
{
  Serial2.IrqHandler();
}

typedef struct
{
  boolean valid;
  char senslopeCommand[50];
  char stationName[10];
} Senslope;
Senslope sensCommand;

typedef struct
{
  boolean valid;
  char sensorA[20];
  char sensorB[20];
  char sensorC[20];
  char sensorD[20];
  char sensorE[20];
  char sensorF[20];
} SensorName;
SensorName loggerName;

typedef struct
{
  boolean valid;
  char inputNumber[50];
} serNumber;
serNumber flashServerNumber;

// typedef struct
// {
//   boolean valid;
//   char simNetwork[50];
// } serNetwork;
// serNetwork flashSimNetwork;

typedef struct
{
  boolean valid;
  char keyword[50];
} smsPassword;
smsPassword flashPassword;

typedef struct
{
  boolean valid;
  char accel_param[100];
  char magneto_param[100];
  char gyro_param[15];
} imu_calib;

/**
 * Reserve a portion of flash memory to store an "int" variable
 * and call it "alarmStorage".
 */
FlashStorage(alarmStorage, int);
FlashStorage(loggerVersion, int);
FlashStorage(imuRawCalib, int);
FlashStorage(gsmPower, int);
FlashStorage(rainCollectorType, int);
FlashStorage(passCommand, Senslope);
FlashStorage(newServerNum, serNumber);
// FlashStorage(simNetwork, serNetwork);
FlashStorage(flashLoggerName, SensorName);
FlashStorage(flashPasswordIn, smsPassword);
FlashStorage(flash_imu_calib, imu_calib);


void setup()
{
  Serial.begin(BAUDRATE);
  DUESerial.begin(DUEBAUD);
  GSMSerial.begin(115200);

  /* Assign pins 10 & 11 UART SERCOM functionality */
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  Wire.begin();
  rtc.begin();
  init_lora();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DUETRIG, OUTPUT);
  pinMode(GSMPWR, OUTPUT);
  pinMode(GSMRST, OUTPUT);
  pinMode(IMU_POWER, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(DUETRIG, LOW);
  digitalWrite(GSMPWR, LOW);
  digitalWrite(GSMRST, HIGH);
  digitalWrite(IMU_POWER, LOW);

//  /* rain gauge interrupt */
//  attachInterrupt(RAININT, rainISR, FALLING);
  /* rtc interrupt */
  attachInterrupt(RTCINTPIN, wake, FALLING);
  /* ring interrupt */
  attachInterrupt(GSMINT, ringISR, FALLING);
  init_Sleep(); // initialize MCU sleep state
  setAlarmEvery30(alarmFromFlashMem()); // rtc alarm settings
  rf95.sleep();

  delay_millis(3000);
  if ((get_logger_version() == 2) || (get_logger_version() == 6) || (get_logger_version() == 10) || (get_logger_version() == 8))
  {
    Serial.println("- - - - - - - - - - - - - - - -");
    Serial.print("Logger Version: ");
    Serial.println(get_logger_version());
    Serial.println("Dafault to LoRa communication.");
    Serial.println("- - - - - - - - - - - - - - -! -");
  }
  else
  {
    // GSM power related
    Serial.println("- - - - - - - - - -");
    Serial.print("Logger Version: ");
    Serial.println(get_logger_version());
    Serial.println("Default to GSM.");
    Serial.println("- - - - - - - - - -");
    if (get_gsm_power_mode() == 1)
    {
      resetGSM();
      gsm_network_connect();
      init_gsm();
      turn_OFF_GSM(get_gsm_power_mode());
    }
    else if (get_gsm_power_mode() == 2)
    {
      resetGSM();
      gsm_network_connect();
      init_gsm();
    }
    else
    {
      resetGSM();
    }
  }

  /*Enter DEBUG mode within 10 seconds*/
  Serial.println("Press 'C' to go DEBUG mode!");
  unsigned long serStart = millis();
  while (serial_flag == 0)
  {
    if (Serial.available())
    {
      debug_flag = 1;
      Serial.println("Debug Mode!");
      // turn_ON_GSM(get_gsm_power_mode());
      serial_flag = 1;
    }
    // timeOut in case walang serial na makuha in ~10 seconds
    if ((millis() - serStart) > 10000)
    {
      serStart = millis();
      serial_flag = 1;
    }
  }
  flashLed(LED_BUILTIN, 5, 60);
}

void loop()
{
  while (debug_flag == 1)
  {
    getAtcommand();
    if (debug_flag_exit)
    {
      Serial.println("* * * * * * * * * * * * *");
      Serial.println("Exiting from DEBUG MENU.");
      Serial.println("* * * * * * * * * * * * *");
      turn_OFF_GSM(get_gsm_power_mode());
      debug_flag = 0;
    }
  }

  if (OperationFlag)
  {
    flashLed(LED_BUILTIN, 2, 50);
    enable_watchdog();

    if (get_logger_version() == 1)
    {
      if (gsmPwrStat)
      {
        turn_ON_GSM(get_gsm_power_mode());
        Watchdog.reset();
      }
      get_Due_Data(1, get_serverNum_from_flashMem());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      if (getSensorDataFlag == true && OperationFlag == true)
      {
        receive_lora_data(1);
        // receive_lora_data(4);
        Watchdog.reset();
      }
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      if (gsmPwrStat)
      {
        turn_OFF_GSM(get_gsm_power_mode());
        Watchdog.reset();
      }
    }
    // else if (get_logger_version() == 99)
    // {
    //   if (gsmPwrStat)
    //   {
    //     turn_ON_GSM(get_gsm_power_mode());
    //     Watchdog.reset();
    //   }
    //   get_Due_Data(1, get_serverNum_from_flashMem());
    //   Watchdog.reset();
    //   send_rain_data(0);
    //   Watchdog.reset();
    //   if (getSensorDataFlag == true && OperationFlag == true)
    //   {
    //     // receive_lora_data(1);
    //     receive_lora_data(1);
    //     Watchdog.reset();
    //   }
    //   attachInterrupt(RTCINTPIN, wake, FALLING);
    //   Watchdog.reset();
    //   if (gsmPwrStat)
    //   {
    //     turn_OFF_GSM(get_gsm_power_mode());
    //     Watchdog.reset();
    //   }
    // }
    else if (get_logger_version() == 2)
    {
      // replaced to experimental code, config for ublox
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      receive_ublox_data_v2(3, "TESUA");
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }
    else if (get_logger_version() == 3)
    {
      // only one trasmitter -- *set for sinua: edit; sending sensor data using gsm only (tesua)
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      receive_ublox_data(3);
      Watchdog.reset();
      //if (dueSamplingTime() == 1){
      //  get_Due_Data(1, get_serverNum_from_flashMem());
      //  Watchdog.reset();
      //  send_rain_data(0);
      //}
      //Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }
    else if (get_logger_version() == 4)
    {
      // Two transmitter
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      receive_lora_data(4);
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }
    else if (get_logger_version() == 5)
    {
      // Three transmitter
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      receive_lora_data(5);
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }
    else if (get_logger_version() == 6)
    {
      // default arabica LoRa transmitter
      get_Due_Data(6, get_serverNum_from_flashMem());
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
    }
    else if (get_logger_version() == 7)
    {
      // Sends rain gauge data via LoRa
      get_Due_Data(0, get_serverNum_from_flashMem());
      Watchdog.reset();
      delay_millis(1000);
      Watchdog.reset();
      send_rain_data(1);
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
    }
    else if (get_logger_version() == 8)
    {
      // Sends rain gauge data via LoRa
      get_Due_Data(0, get_serverNum_from_flashMem());
      Watchdog.reset();
      delay_millis(1000);
      Watchdog.reset();
      send_rain_data(1);
      Watchdog.reset();
      send_thru_lora(dataToSend);
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
    }
    else if (get_logger_version() == 9)
    {
      // Sends IMU sensor data to GSM
      /*
      on_IMU();
      Watchdog.reset();
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      delay_millis(1000);
      Watchdog.reset();
      send_thru_gsm(read_IMU_data(get_calib_param()), get_serverNum_from_flashMem());
      Watchdog.reset();
      delay_millis(1000);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
      off_IMU();
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      */
    }
    else if (get_logger_version() == 10)
    {
      // LoRa transmitter of version 5 datalogger
      get_Due_Data(2, get_serverNum_from_flashMem());
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      // Sends IMU sensor data to LoRa
      /*
      on_IMU();
      Watchdog.reset();
      send_thru_lora(read_IMU_data(get_calib_param()));
      Watchdog.reset();
      // delay_millis(1000);
      // Watchdog.reset();
      // send_rain_data(1);
      // Watchdog.reset();
      off_IMU();
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      */
    }
    else if (get_logger_version() == 11)
    {
      // Sends rain gauge data ONLY
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      delay_millis(1000);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
    }
    else if (get_logger_version() == 12)
    {
      // Sends rain gauge data ONLY
      // turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      if (gsmPwrStat)
      {
        turn_ON_GSM(get_gsm_power_mode());
        Watchdog.reset();
      }
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      delay_millis(3000);
      Watchdog.reset();
      send_thru_gsm(dataToSend, "639161640761");
      // send_thru_gsm(dataToSend, "639175388301");
      Watchdog.reset();
      delay_millis(1000);
      Watchdog.reset();
      // turn_OFF_GSM(get_gsm_power_mode());
      if (gsmPwrStat)
      {
        turn_OFF_GSM(get_gsm_power_mode());
        Watchdog.reset();
      }
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      send_rain_data_flag = true;
    }
    else if (get_logger_version() == 13)
    {
      // UBLOX gateway SINTB
      digitalWrite(LED_BUILTIN, HIGH);
      if (gsmPwrStat)
      {
        turn_ON_GSM(get_gsm_power_mode());
        Watchdog.reset();
      }
      get_Due_Data(1, get_serverNum_from_flashMem());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      if (getSensorDataFlag == true && OperationFlag == true)
      {
        receive_lora_data_UBLOX(12);
        Watchdog.reset();
      }
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      if (gsmPwrStat)
      {
        turn_OFF_GSM(get_gsm_power_mode());
        Watchdog.reset();
      }
      digitalWrite(LED_BUILTIN, LOW);
    }
    else if (get_logger_version() == 14)
    {
      // LoRa Gateway with max timeOut only
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      receive_lora_data_ONLY(14);
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }
    else if (get_logger_version() == 15)
    {
      // Three transmitter
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      receive_lora_data(5);
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }
    else
    {
      // default arQ like sending
      turn_ON_GSM(get_gsm_power_mode());
      Watchdog.reset();
      send_rain_data(0);
      Watchdog.reset();
      get_Due_Data(1, get_serverNum_from_flashMem());
      Watchdog.reset();
      attachInterrupt(RTCINTPIN, wake, FALLING);
      Watchdog.reset();
      turn_OFF_GSM(get_gsm_power_mode());
      Watchdog.reset();
    }

    rf95.sleep();
    getSensorDataFlag = false;
    OperationFlag = false;
  }

  if (rainFallFlag)
  {
    if (get_logger_version() == 2)
    {
      // wakeGSM();
      flashLed(LED_BUILTIN, 2, 50);
      // LoRa transmitter of version 5 datalogger
      get_Due_Data(2, get_serverNum_from_flashMem());
      // sleepGSM();
    }
    else
    {
      flashLed(LED_BUILTIN, 1, 50);
      attachInterrupt(RAININT, rainISR, FALLING);
      rainFallFlag = false;
    }
  }

  if (gsmRingFlag)
  {
    flashLed(LED_BUILTIN, 3, 40);
    if (get_gsm_power_mode() == 1)
    {
      Serial.println("1st AT + CNMI");
      GSMSerial.write("AT+CNMI=1,2,0,0,0\r");
      delay_millis(100);
    }
    GSMSerial.write("AT+CNMI=1,2,0,0,0\r");
    delay_millis(300);
    while (GSMSerial.available() > 0)
    {
      processIncomingByte(GSMSerial.read(), 0);
    }
    turn_OFF_GSM(get_gsm_power_mode());
    attachInterrupt(GSMINT, ringISR, FALLING);
    gsmRingFlag = false;
  }

  setAlarmEvery30(alarmFromFlashMem());
  delay_millis(75);
  rtc.clearINTStatus();

  attachInterrupt(GSMINT, ringISR, FALLING);
//  attachInterrupt(RAININT, rainISR, FALLING);
  attachInterrupt(RTCINTPIN, wake, FALLING);
  sleepNow();
}

void enable_watchdog()
{
  Serial.println("Watchdog Enabled!");
  int countDownMS = Watchdog.enable(16000); // max of 16 seconds
}

void disable_watchdog()
{
  Serial.println("Watchdog Disabled!");
  Watchdog.disable();
}

void wakeAndSleep(uint8_t verSion)
{
  if (OperationFlag)
  {
    flashLed(LED_BUILTIN, 5, 100);

    if (verSion == 1)
    {
      get_Due_Data(2, get_serverNum_from_flashMem()); // tx of v5 logger
    }
    else
    {
      get_Due_Data(0, get_serverNum_from_flashMem()); // default arabica
    }

    setAlarmEvery30(alarmFromFlashMem());
    rtc.clearINTStatus(); // needed to re-trigger rtc
    rf95.sleep();
    OperationFlag = false;
  }
  // working as of May 28, 2019
  sleepNow();
  /*
  attachInterrupt(digitalPinToInterrupt(RTCINTPIN), wake, FALLING);
  SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; //disable systick interrupt
  LowPower.standby();                         //enters sleep mode
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;  //Enabale systick interrupt
  */
}

/*Enable sleep-standby*/
void sleepNow()
{
  Serial.println("MCU is going to sleep . . .");
  SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // disable systick interrupt
  LowPower.standby();                         // enters sleep mode
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;  // Enabale systick interrupt
}

/**RTC Pin interrupt
 * hardware interrupt from RTC
 * microcontroller will wake from sleep
 * execute the process
 */
void wake()
{
  OperationFlag = true;
  // detach the interrupt in the ISR so that multiple ISRs are not called
  detachInterrupt(RTCINTPIN);
}

void ringISR()
{
  gsmRingFlag = true;
  detachInterrupt(GSMINT);
}

/**GATEWAY*RSSI,MAD,MADTA,rssi,voltage,MADTB,,,*200212141406
 *main logger name, MADTA, MADTB, . . .
 *send data to gsm
 */
void get_rssi(uint8_t mode)
{
  char convertRssi[20];
  char convertRssiB[20];
  char convertRssiC[20];
  char logger_name[200];
  // String old_rssi = String(tx_RSSI);
  // String old_rssi_B = String(tx_RSSI_B);
  // String old_rssi_C = String(tx_RSSI_C);
  tx_RSSI.toCharArray(convertRssi, sizeof(convertRssi));
  tx_RSSI_B.toCharArray(convertRssiB, sizeof(convertRssiB));
  tx_RSSI_C.toCharArray(convertRssiC, sizeof(convertRssiC));
  readTimeStamp();

  String loggerName = String(get_logger_A_from_flashMem());
  loggerName.replace("\r", "");
  loggerName.remove(3);
  loggerName.toCharArray(logger_name, 200);

  for (int i = 0; i < 250; i++)
    rssiString[i] = 0;
  strncpy(rssiString, "GATEWAY*RSSI,", 13);
  strncat(rssiString, logger_name, sizeof(logger_name));
  strncat(rssiString, ",", 1);
  strncat(rssiString, get_logger_B_from_flashMem(), 20);
  strncat(rssiString, ",", 1);
  strncat(rssiString, convertRssi, 100);
  strncat(rssiString, ",", 1);
  strncat(rssiString, txVoltage, sizeof(txVoltage)); // voltage working 02-17-2020
  // strncat(dataToSend, parse_voltage(received), sizeof(parse_voltage(received)));
  if (mode == 4)
  {
    strncat(rssiString, ",", 1);
    strncat(rssiString, get_logger_C_from_flashMem(), 20);
    strncat(rssiString, ",", 1);
    strncat(rssiString, convertRssiB, 100);
    strncat(rssiString, ",", 1);
    strncat(rssiString, txVoltageB, sizeof(txVoltageB)); // voltage working 02-17-2020
    // strncat(dataToSend, parse_voltage_B(received), sizeof(parse_voltage_B(received)));
  }
  else if (mode == 5)
  {
    strncat(rssiString, ",", 1);
    strncat(rssiString, get_logger_C_from_flashMem(), 20);
    strncat(rssiString, ",", 1);
    strncat(rssiString, convertRssiB, 100);
    strncat(rssiString, ",", 1);
    strncat(rssiString, txVoltageB, sizeof(txVoltageB)); // voltage working 02-17-2020
    strncat(rssiString, ",", 1);
    strncat(rssiString, get_logger_D_from_flashMem(), 20); // sensorD
    strncat(rssiString, ",", 1);
    strncat(rssiString, convertRssiC, 100);
    strncat(rssiString, ",", 1);
    strncat(rssiString, txVoltageC, sizeof(txVoltageC));
  }
  else if (mode == 12)
  {
    strncat(rssiString, ",", 1);
    strncat(rssiString, get_logger_C_from_flashMem(), 20);
    strncat(rssiString, ",", 1);
    strncat(rssiString, convertRssiB, 100);
    strncat(rssiString, ",", 1);
    strncat(rssiString, txVoltageB, sizeof(txVoltageB)); // voltage working 02-17-2020
    strncat(rssiString, ",", 1);
    strncat(rssiString, get_logger_D_from_flashMem(), 20); // sensorD
    strncat(rssiString, ",", 1);
    strncat(rssiString, convertRssiC, 100);
    strncat(rssiString, ",", 1);
    strncat(rssiString, txVoltageC, sizeof(txVoltageC));
  }
  strncat(rssiString, ",*", 2);
  strncat(rssiString, Ctimestamp, sizeof(Ctimestamp));
  delay_millis(500);
  send_thru_gsm(rssiString, get_serverNum_from_flashMem());

  // clear RSSI values
  tx_RSSI = "";
  tx_RSSI_B = "";
  tx_RSSI_C = "";
}

/**
 * Get data allocated from flash memory
 */
char *stationName_from_flashMem()
{
  String get_cmd;
  char new_cmd[10];
  sensCommand = passCommand.read();
  get_cmd = sensCommand.stationName;
  get_cmd.replace("\r", "");
  get_cmd.toCharArray(new_cmd, 10);
  return new_cmd;
}

char *get_logger_A_from_flashMem()
{
  loggerName = flashLoggerName.read();
  return loggerName.sensorA;
}

char *get_logger_B_from_flashMem()
{
  loggerName = flashLoggerName.read();
  return loggerName.sensorB;
}

char *get_logger_C_from_flashMem()
{
  loggerName = flashLoggerName.read();
  return loggerName.sensorC;
}

char *get_logger_D_from_flashMem()
{
  loggerName = flashLoggerName.read();
  return loggerName.sensorD;
}

char *get_logger_E_from_flashMem()
{
  loggerName = flashLoggerName.read();
  return loggerName.sensorE;
}

char *get_logger_F_from_flashMem()
{
  loggerName = flashLoggerName.read();
  return loggerName.sensorF;
}

String get_serverNum_from_flashMem()
{
  String flashNum;
  flashServerNumber = newServerNum.read();
  flashNum = flashServerNumber.inputNumber;
  flashNum.replace("\r", "");
  return flashNum;
}

// String get_serverNetwork_from_flashMem()
// {
//   String flashNet;
//   flashSimNetwork = simNetwork.read();
//   flashNet = flashSimNetwork.simNetwork;
//   flashNet.replace("\r", "");
//   return flashNet;
// }

char *get_password_from_flashMem()
{
  flashPassword = flashPasswordIn.read();
  return flashPassword.keyword;
}

/**sendTo
 * 0 - default for GSM sending
 * 1 - send to LoRa with >> and << added to data
 */
void send_rain_data(uint8_t sendTo)
{
  disable_watchdog();
  char temp[10];
  char volt[10];
  readTimeStamp();

  for (int i = 0; i < DATALEN; i++)
    dataToSend[i] = 0;
  /* MADTAW,32.00,0.00,0.82,99,200415171303
      station name, temperature, raintips, input voltage, gsm csq */
  if (sendTo == 1)
  {
    strncpy(dataToSend, ">>", 2);
    strncat((dataToSend), (get_logger_A_from_flashMem()), (20));
  }
  else
  {
    strncpy((dataToSend), (get_logger_A_from_flashMem()), (20));
  }
  // strncpy((dataToSend), (get_logger_A_from_flashMem()), (20));
  strncat(dataToSend, "W", 1);
  strncat(dataToSend, ",", 1);

  snprintf(temp, sizeof temp, "%.2f", readTemp());
  strncat(dataToSend, temp, sizeof(temp));
  strncat(dataToSend, ",", 1);

  snprintf(sendRainTip, sizeof sendRainTip, "%.2f", rainTips);
  strncat(dataToSend, sendRainTip, sizeof(sendRainTip));
  strncat(dataToSend, ",", 1);

  snprintf(volt, sizeof volt, "%.2f", readBatteryVoltage(get_calib_param()));
  strncat(dataToSend, volt, sizeof(volt));

  strncat(dataToSend, ",", 1);
  strncat(dataToSend, readCSQ(), sizeof(readCSQ()));
  // strncat(dataToSend, _csq, sizeof(_csq));
  strncat(dataToSend, ",", 1);
  strncat(dataToSend, Ctimestamp, sizeof(Ctimestamp));
  if (get_logger_version() == 6)
  {
    strncat(dataToSend, "<<", 2);
  }
  delay_millis(500);
  if (sendTo == 1)
  {
    send_thru_lora(dataToSend);
  }
  else
  {
    send_thru_gsm(dataToSend, get_serverNum_from_flashMem());
    delay_millis(500);
    resetRainTips();
  }
  send_rain_data_flag = false;
  enable_watchdog();
}

void flashLed(int pin, int times, int wait)
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(pin, HIGH);
    delay_millis(wait);
    digitalWrite(pin, LOW);
    if (i + 1 < times)
    {
      delay_millis(wait);
    }
  }
}

char *read_batt_vol(uint8_t ver)
{
  char volt[10];
  for (int i = 0; i < 200; i++)
    voltMessage[i] = 0;
  // dtostrf((readBatteryVoltage(get_logger_version())), 4, 2, volt);
  snprintf(volt, sizeof volt, "%.2f", readBatteryVoltage(ver));
  readTimeStamp();

  if (ver == 6)
  {
    strncpy(voltMessage, ">>", 2);
  }
  else
  {
    strncpy(voltMessage, get_logger_A_from_flashMem(), 20);
  }
  strncat(voltMessage, "*VOLT:", 7);
  strncat(voltMessage, volt, sizeof(volt));
  strncat(voltMessage, "*", 1);
  if (ver == 6)
  {
    strncat(voltMessage, Ctimestamp, sizeof(Ctimestamp));
    strncat(voltMessage, "<<", 2);
  }
  else
  {
    strncat(voltMessage, Ctimestamp, sizeof(Ctimestamp));
  }
  return voltMessage;
}

// Measure battery voltage using divider on Feather M0
float readBatteryVoltage(uint8_t ver)
{
  float measuredvbat;
  // if ((ver == 3) || (ver == 9) || (ver == 10) || (ver == 11))
  if (ver == 1)
  {
    // 4.2 volts
    measuredvbat = analogRead(VBATPIN); // Measure the battery voltage at pin A7
    measuredvbat *= 2;                  // we divided by 2, so multiply back
    measuredvbat *= 3.3;                // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024;               // convert to voltage
    measuredvbat += 0.15;               // between 0.15 and 0.4 drop in schottky diode
  }
  else
  {
    /* Voltage Divider 1M and  100k */
    // 12 volts
    measuredvbat = analogRead(VBATEXT);
    measuredvbat *= 3.3;    // reference voltage
    measuredvbat /= 1024.0; // adc max count
    measuredvbat *= 11.0;   // (100k+1M)/100k
  }
  return measuredvbat;
}

/*
  ** interrupts EIC
  EXTERNAL_INT_2: A0, A5, 10
  EXTERNAL_INT_4: A3, 6
  EXTERNAL_INT_5: A4, 7
  EXTERNAL_INT_6: 8, SDA
  EXTERNAL_INT_7: 9, SCL
  EXTERNAL_INT_9: A2, 3
  EXTERNAL_INT_10: TX, MOSI
  EXTERNAL_INT_11: RX, SCK
*/
void init_Sleep()
{
  // working to as of 05-17-2019
  SYSCTRL->XOSC32K.reg |= (SYSCTRL_XOSC32K_RUNSTDBY | SYSCTRL_XOSC32K_ONDEMAND); // set external 32k oscillator to run when idle or sleep mode is chosen
  REG_GCLK_CLKCTRL |= GCLK_CLKCTRL_ID(GCM_EIC) |                                 // generic clock multiplexer id for the external interrupt controller
                      GCLK_CLKCTRL_GEN_GCLK1 |                                   // generic clock 1 which is xosc32k
                      GCLK_CLKCTRL_CLKEN;                                        // enable it
  while (GCLK->STATUS.bit.SYNCBUSY)
    ; // write protected, wait for sync

  EIC->WAKEUP.reg |= EIC_WAKEUP_WAKEUPEN4; // Set External Interrupt Controller to use channel 4 (pin 6)
  EIC->WAKEUP.reg |= EIC_WAKEUP_WAKEUPEN5; // Set External Interrupt Controller to use channel 2 (pin A4)
  // EIC->WAKEUP.reg |= EIC_WAKEUP_WAKEUPEN2; // channel 2 (pin A0)

  PM->SLEEP.reg |= PM_SLEEP_IDLE_CPU; // Enable Idle0 mode - sleep CPU clock only
  // PM->SLEEP.reg |= PM_SLEEP_IDLE_AHB; // Idle1 - sleep CPU and AHB clocks
  // PM->SLEEP.reg |= PM_SLEEP_IDLE_APB; // Idle2 - sleep CPU, AHB, and APB clocks

  // It is either Idle mode or Standby mode, not both.
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; // Enable Standby or "deep sleep" mode
}

/**
 * Get custom due sensor data.
 * ~5 minutes timeout if no data read.
 * mode in sending data: 1-gsm ; 0 - LoRa(defualt) ; 2 - V5 logger
 * 
 * mode 1: basa sa serial ng DUE - tapos pasa sa send_thru_gsm()
 *
 */
void get_Due_Data(uint8_t mode, String serverNum)
{
  DUESerial.begin(DUEBAUD);
  disable_watchdog();
  unsigned long start = millis();

  /*Serial.println("starting delay please wait . . .");
  // delay 1 minute before getting due data
  delay(DUEDELAY);
  Serial.println("DUE delay reached!");*/

  readTimeStamp();
  turn_ON_due(get_logger_version());
  delay_millis(500);

  sensCommand = passCommand.read();
  command[0] = '\0';
  strncpy((command), (sensCommand.senslopeCommand), (10));
  strncat(command, "/", 1);
  strncat(command, Ctimestamp, sizeof(Ctimestamp));
  Serial.println(command);
  DUESerial.write(command);
  Serial.println("Waiting for SINSA data. . .");

  while (customDueFlag == 0)
  {
    // timeOut in case walang makuhang data sa due
    if ((millis() - start) > DUETIMEOUT)
    {
      start = millis();
      no_data_from_senslope(mode);
      customDueFlag = 1;
    }

    for (int i = 0; i < 250; ++i)
      streamBuffer[i] = 0x00;
    DUESerial.readBytesUntil('\n', streamBuffer, 250);
    delay_millis(500);

    if (strstr(streamBuffer, ">>"))
    {
      if (strstr(streamBuffer, "*"))
      {
        Serial.println("Getting sensor data. . .");
        if (mode == 0 || mode == 1)
        {
          /**
           * Remove 1st and 2nd character data in string
           * Not needed in GSM mode
           */
          for (byte i = 0; i < strlen(streamBuffer); i++)
          {
            streamBuffer[i] = streamBuffer[i + 2];
          }
          // send_thru_gsm(streamBuffer, get_serverNum_from_flashMem());
          send_thru_gsm(streamBuffer, serverNum);
          flashLed(LED_BUILTIN, 2, 100);
          DUESerial.write("OK");
        }
        else if (mode == 6 || mode == 7)
        {
          strncat(streamBuffer, "<<", 2);
          delay(10);
          send_thru_lora(streamBuffer);
          flashLed(LED_BUILTIN, 2, 100);
          DUESerial.write("OK");
          // send_thru_lora(streamBuffer);
          // flashLed(LED_BUILTIN, 2, 100);
          // DUESerial.write("OK");
        }
        else
        {
          send_thru_lora(streamBuffer);
          flashLed(LED_BUILTIN, 2, 100);
          DUESerial.write("OK");
        }
      }
      else
      {
        // maglagay ng counter max 5 then exit
        Serial.println("Message incomplete");
        DUESerial.write("NO");
      }
    }
    else if (strstr(streamBuffer, "STOPLORA"))
    {
      /*if (mode == 0 || mode == 2)
      {
        delay(1000);
        send_thru_lora(read_batt_vol(mode));
        delay(1500); //needed for the gsm to wait until sending
        send_thru_lora("STOPLORA");
      }*/
      Serial.println("Done getting DUE data!");
      streamBuffer[0] = '\0';
      customDueFlag = 1;
    }
  }
  if (mode == 2 || mode == 6 || mode == 7 || mode == 8)
  {
    delay_millis(2000);
    send_thru_lora(read_batt_vol(get_calib_param()));
  }
  turn_OFF_due(get_logger_version());
  DUESerial.end();
  flashLed(LED_BUILTIN, 4, 90);
  customDueFlag = 0;
  getSensorDataFlag = true;
  enable_watchdog();
}

/**
 * Sends no data from senslope if no data available
 * mode :     1 - gsm
 * default:   0 - LoRa
 *Serial.println("[0] Sendng data using GSM only");
  Serial.println("[1] Version 5 datalogger LoRa with GSM");
  Serial.println("[2] LoRa transmitter for version 5 datalogger");
  Serial.println("[3] Gateway Mode with only ONE LoRa transmitter");
  Serial.println("[4] Gateway Mode with TWO LoRa transmitter");
  Serial.println("[5] Gateway Mode with THREE LoRa transmitter");
  Serial.println("[6] LoRa transmitter for Raspberry Pi");
  Serial.println("[7] Sends rain gauge data via LoRa");
*/
void no_data_from_senslope(uint8_t mode)
{
  readTimeStamp();
  sensCommand = passCommand.read(); // read from flash memory
  Serial.println("No data from senslope");
  streamBuffer[0] = '\0';

  if (mode == 1 || mode == 0)
  {
    strncpy((streamBuffer), (get_logger_A_from_flashMem()), (20));
  }
  else
  {
    strncpy(streamBuffer, ">>", 2);
    strncat((streamBuffer), (get_logger_A_from_flashMem()), (20));
  }

  strncat(streamBuffer, "*NODATAFROMSENSLOPE*", 22);
  strncat(streamBuffer, Ctimestamp, sizeof(Ctimestamp));

  if (mode == 1 || mode == 0)
  {
    send_thru_gsm(streamBuffer, get_serverNum_from_flashMem());
  }
  else if (mode == 2)
  {
    send_thru_lora(streamBuffer);
  }
  else
  {
    strncat(streamBuffer, "<<", 2);
    send_thru_lora(streamBuffer);
  }
  customDueFlag = 1;
}

void turn_ON_due(uint8_t mode)
{
  Serial.println("Turning ON Custom Due. . .");
  digitalWrite(DUETRIG, HIGH);
  delay_millis(100);
}

void turn_OFF_due(uint8_t mode)
{
  Serial.println("Turning OFF Custom Due. . .");
  digitalWrite(DUETRIG, LOW);
  delay_millis(100);
}

void rainISR()
{
  detachInterrupt(digitalPinToInterrupt(RAININT));
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > DEBOUNCE_TIME)
  {
    if (get_rainCollector_type() == 0)
    {
      rainTips += 0.5;
    }
    else if (get_rainCollector_type() == 1)
    {
      rainTips += 0.2;
    }
    else
    {
      rainTips += 1;
    }
  }
  last_interrupt_time = interrupt_time;
  if (OperationFlag == true || debug_flag == 1)
  {
    attachInterrupt(RAININT, rainISR, FALLING);
  }
  else
  {
    rainFallFlag = true;
  }
}

void resetRainTips()
{
  rainTips = 0.00;
  delay_millis(75);
  dtostrf(rainTips, 3, 2, sendRainTip); // convert rainTip to char
  Serial.print("Rain tips: ");
  Serial.println(rainTips);
}

void send_rain_tips()
{
  send_rain_data(0);
}

String parse_voltage(char *toParse)
{
  int i = 0;
  String parse_volt;

  // MADTB*VOLT:12.33*200214111000
  char *buff = strtok(toParse, ":");
  while (buff != 0)
  {
    char *separator = strchr(buff, '*');
    if (separator != 0)
    {
      *separator = 0;
      if (i == 1) // 2nd appearance
      {
        parse_volt = buff;
      }
      i++;
    }
    buff = strtok(0, ":");
  }
  return parse_volt;
}

void delay_millis(int _delay)
{
  uint8_t delay_turn_on_flag = 0;
  unsigned long _delayStart = millis();
  // Serial.println("starting delay . . .");
  do
  {
    if ((millis() - _delayStart) > _delay)
    {
      _delayStart = millis();
      delay_turn_on_flag = 1;
      // Serial.println("delay timeout!");
    }
  } while (delay_turn_on_flag == 0);
}
