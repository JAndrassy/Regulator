#ifndef H_CONSTS_H
#define H_CONSTS_H

const char version[] = "build "  __DATE__ " " __TIME__;

#define FILE_NEW (O_READ | O_WRITE | O_CREAT)

#ifdef ARDUINO_SAMD_NANO_33_IOT // on Nano Grove Shield
const byte MAIN_RELAY_PIN = 0; // D0+D1 connector (Serial1)
const byte BYPASS_RELAY_PIN = 1; //
const byte BALBOA_RELAY_PIN = 2;  // D2+D3
const byte VALVES_RELAY_PIN = 3;
const byte ZC_EI_PIN = 4; // D4+D5
const byte TRIAC_PIN = 5;
const byte TONE_PIN = 6; // D6+D7

const byte ELSENS_PIN = A0; // A0+A1 connector
const byte BUTTON_PIN = A2; //A2+A3
// A4,A5 I2C
const byte LEDBAR_DATA_PIN = A6; // A6+A7
const byte LEDBAR_CLOCK_PIN = A7;

// not on Grove Shield:
// 8 and 9 // free
const byte SD_SS_PIN = 10;
// SPI 11, 12, 13

#elif defined(ARDUINO_SAMD_MKRZERO) || defined(ARDUINO_SAMD_MKR1000) || defined(ARDUINO_SAMD_MKRWIFI1010)
// on MKR Connector Carrier
const byte TONE_PIN = 0;
const byte BALBOA_RELAY_PIN  = 1;
const byte VALVES_RELAY_PIN = 2;
const byte MAIN_RELAY_PIN = 3;
const byte BYPASS_RELAY_PIN = 4;
const byte ZC_EI_PIN = 5;  // on same connector with 6
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib

#ifdef ARDUINO_SAMD_MKRZERO
const byte NET_SS_PIN = 7;  // not on Carrier
#else
#undef SDCARD_SS_PIN
#define SDCARD_SS_PIN 7
#endif
// SPI 8, 9, 10 not on Carrier
// TWI 12, 11
#ifdef ARDUINO_SAMD_MKRZERO // doesn't have on board I2C devices and may use Serial1 for WiFi
const byte LEDBAR_DATA_PIN = 12; // connector labeled TWI
const byte LEDBAR_CLOCK_PIN = 11; //on one Grove connector
#else
const byte LEDBAR_DATA_PIN = 13; // connector labeled Serial (it is for Serial1)
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector
#endif

const byte ELSENS_PIN = A1; // the capacitor next to voltage divider needs to be removed
const byte BUTTON_PIN = A4; // with voltage divider removed

const byte SD_SS_PIN = SDCARD_SS_PIN; // internal pin on MKR ZERO

#else // boards with Grove Uno base shield

const byte TONE_PIN = 2;
#ifdef ARDUINO_SAMD_ZERO
const byte MAIN_RELAY_PIN = 3;
const byte SD_SS_PIN = 4;  // Ethernet shield
const byte ZC_EI_PIN = 5;  // on one Grove connector with TRIAC_PIN
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib
const byte NET_SS_PIN = 10; // Ethernet shield
#elif defined(PROBADIO)
const byte ZC_EI_PIN = 3; // INT1 pin. on one Grove connector with TRIAC_PIN
const byte TRIAC_PIN = 4; // TIMER1 OC1B
const byte MAIN_RELAY_PIN = 5;
const byte BYPASS_RELAY_PIN = 6;
const byte SD_SS_PIN = 10;  // Adafruit SD card adapter directly on pins 10 to 13
//pin 10-13 SPI (Ethernet, SD)
const byte NET_SS_PIN = A5; // is close to SPI header
#elif defined(ARDUINO_AVR_MEGA2560)
const byte ZC_EI_PIN = 3; // INT1 pin
const byte SD_SS_PIN = 4;
const byte MAIN_RELAY_PIN = 5;
const byte BYPASS_RELAY_PIN = 6;
const byte TRIAC_PIN = 11; // TIMER1 OC1A
#endif
const byte BUTTON_PIN = 7;
const byte LEDBAR_DATA_PIN = 8;
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector

// A0 free
const byte ELSENS_PIN = A1;
const byte BALBOA_RELAY_PIN = A2;
const byte VALVES_RELAY_PIN = A3;

#endif

const byte STATUS_LED_PIN = 99; // status led not used

const int MAX_POWER = 2100;
const int BYPASS_POWER = 2100;
const unsigned long PUMP_STOP_MILLIS = 10 * 60000; // 10 min
const unsigned long VALVE_ROTATION_TIME = 30000; // 30 sec

const IPAddress symoAddress(192,168,1,7);

enum struct RegulatorState {
  REST = 'N', // night
  ALARM = 'A',
  MONITORING = 'M', // SoC < xx%
  REGULATING = 'R', // PowerPilot.ino
  OVERHEATED = 'O', // ElSesns.ino heater safety thermostat triggered
  MANUAL_RUN = 'H'  // ManualRun.ino - H as 'by hand'
};

enum struct AlarmCause {
  NOT_IN_ALARM = '-',
  NETWORK = 'N',
  MODBUS = 'M',
  PUMP = 'P'
};

// events array indexes (and size)
enum {
  EVENTS_SAVE_EVENT,
  RESTART_EVENT,
  WATCHDOG_EVENT,
  NETWORK_EVENT,
  PUMP_EVENT,
  MODBUS_EVENT,
  OVERHEATED_EVENT,
  BALBOA_PAUSE_EVENT,
  MANUAL_RUN_EVENT,
  VALVES_BACK_EVENT,
  SUSPEND_CALIBRATION_EVENT,
  BATTSETT_LIMIT_EVENT,
  POWERPILOT_PLAN_EVENT,
  STATS_SAVE_EVENT,
  EVENTS_SIZE
};

enum {
  BATTERY_PRIORITY,
  HEATING_PRIORITY_AM,
  HEATING_DISABLED_AM,
  HEATING_DISABLED
};

struct Stats {
  long heatingTime = 0; // minutes
  long consumedPower = 0; // watts
};

#endif
