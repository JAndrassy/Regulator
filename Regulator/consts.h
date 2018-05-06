#ifndef H_CONSTS_H
#define H_CONSTS_H

#include "secrets.h"

const char version[] = "build "  __DATE__ " " __TIME__;

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
  OVERHEAT_EVENT,
  BALBOA_PAUSE_EVENT,
  MANUAL_RUN_EVENT,
  VALVES_BACK_EVENT,
  SUSPEND_CALIBRATION_EVENT,
  EVENTS_SIZE
};

#endif
