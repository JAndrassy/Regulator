#ifndef H_CONSTS_H
#define H_CONSTS_H

const char version[] = "build "  __DATE__ " " __TIME__;

#define FILE_NEW (O_READ | O_WRITE | O_CREAT)

const byte TONE_PIN = 2;
#ifdef ARDUINO_SAMD_ZERO
const byte MAIN_RELAY_PIN = 3;
const byte SD_SS_PIN = 4;  // Ethernet shield
const byte ZC_EI_PIN = 5;  // on one Grove connector with TRIAC_PIN
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib
const byte NET_SS_PIN = 10; // Ethernet shield
// const byte PUMP_RELAY_PIN = ??;
#elif defined(PROBADIO)
const byte ZC_EI_PIN = 3; // INT1 pin. on one Grove connector with TRIAC_PIN
const byte TRIAC_PIN = 4; // TIMER1 OC2A
const byte MAIN_RELAY_PIN = 5;
const byte PUMP_RELAY_PIN = 6;
const byte SD_SS_PIN = 10;  // Adafruit SD card adapter directly on pins 10 to 13
//pin 10-13 SPI (Ethernet, SD)
const byte NET_SS_PIN = A5; // is close to SPI header
#endif
const byte BYPASS_RELAY_PIN = 7;
const byte LEDBAR_DATA_PIN = 8;
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector
const byte BUTTON_PIN = SCL; // Grove Base shield I2C connector

// A0 free
const byte ELSENS_PIN = A1;
const byte BALBOA_RELAY_PIN = A2;
const byte VALVES_RELAY_PIN = A3;

const byte STATUS_LED_PIN = 99; // status led not used

const int MAX_POWER = 2050;
const int BYPASS_POWER = 2050;
const unsigned long PUMP_STOP_MILLIS = 10 * 60000; // 10 min

const IPAddress symoAddress(192,168,1,7);

enum struct RegulatorState {
  REST = 'N', // night
  ALARM = 'A',
  MONITORING = 'M', // SoC < xx%
  REGULATING = 'R', // PowerPilot.ino
  OVERHEATED = 'O', // ElSesns.ino heater safety thermostat triggered
  MANUAL_RUN = 'H', // ManualRun.ino - H as 'by hand'
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
