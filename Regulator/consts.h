#ifndef H_CONSTS_H
#define H_CONSTS_H

#include "secrets.h"

const char version[] = "build "  __DATE__ " " __TIME__;

#ifdef ESP8266
#include "gbs-d1r2.h"
#define FILE_WRITE "a"
#define FILE_READ "r"

const byte BUTTON_PIN = GBS_RX_io1_RX0;
const byte MAIN_RELAY_PIN = GBS_D2_io16;
// D3
// D4
const byte BYPASS_RELAY_PIN = GBS_D5_io0_PULLUP;
const byte TONE_PIN = GBS_D6_io2_PULLUP;
const byte PWM_PIN = GBS_D7_io14;
const byte LEDBAR_DATA_PIN = GBS_D8_io12;
const byte LEDBAR_CLOCK_PIN = GBS_D9_io13;
//const byte TEMPSENS_PIN = A0;
const byte ELSENS_PIN = A0;
const byte BALBOA_RELAY_PIN = GBS_A2_io15_PULLDOWN; // jumper wire from pin 10 to unused A2
//const byte VALVES_RELAY_PIN = GBS_A3_io3_TX0; // with jumper wire on GBS headers TX->A3
#else
const byte MAIN_RELAY_PIN = 2;
const byte TONE_PIN = 3;
const byte SD_SS_PIN = 4; // Ethernet shield
const byte BYPASS_RELAY_PIN = 5;
const byte PWM_PIN = 6;
const byte BUTTON_PIN = 7;
const byte LEDBAR_DATA_PIN = 8;
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector
//pin 10-13 SPI (Ethernet shield)
const byte ELSENS_PIN = A0;
const byte TEMPSENS_PIN = A1;
const byte BALBOA_RELAY_PIN = A2;
const byte VALVES_RELAY_PIN = A3;
//pin A4, A5 is I2C (on Uno Wifi ESP8266 over I2C SC)
#endif

const int PUMP_POWER = 44;
const int MAX_POWER = 1990;
const int BYPASS_POWER = 2050;
const unsigned long PUMP_STOP_MILLIS = 5 * 60000; // 5 min

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
  WIFI = 'W',
  MODBUS = 'M',
  PUMP = 'P'
};

// events array indexes (and size)
enum {
  EEPROM_EVENT,
  RESTART_EVENT,
  WATCHDOG_EVENT,
  WIFI_EVENT,
  PUMP_EVENT,
  MODBUS_EVENT,
  OVERHEATED_EVENT,
  BALBOA_PAUSE_EVENT,
  MANUAL_RUN_EVENT,
  VALVES_BACK_EVENT,
  SUSPEND_CALIBRATION_EVENT,
  BATTSETT_LIMIT_EVENT,
  EVENTS_SIZE
};

#endif
