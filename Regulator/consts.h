#ifndef H_CONSTS_H
#define H_CONSTS_H

#include "secrets.h"

const char version[] = "build "  __DATE__ " " __TIME__;

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
#define NO_EEPROM
#endif


#ifdef ESP8266
#include "gbs-d1r2.h"
#define FILE_WRITE "a"
#define FILE_READ "r"
#define FILE_NEW "w"
#define EEPROM_SIZE 512

const byte BUTTON_PIN = GBS_RX_io1_RX0;
const byte MAIN_RELAY_PIN = GBS_D2_io16;
//I2C GBS_D3_io5_I2C_SCL
//I2C GBS_D4_io4_I2C_SCA
const byte BYPASS_RELAY_PIN = GBS_D5_io0_PULLUP;
const byte TONE_PIN  = GBS_D6_io2_PULLUP; // relay or mosfet module on io2 block boot
const byte PWM_PIN = GBS_D7_io14;
const byte LEDBAR_DATA_PIN = GBS_D8_io12;
const byte LEDBAR_CLOCK_PIN = GBS_D9_io13;
const byte TEMPSENS_PIN = A0;
//const byte ELSENS_PIN = A0;
const byte BALBOA_RELAY_PIN = GBS_A2_io15_PULLDOWN; // jumper wire from pin 10 to unused A2
const byte VALVES_RELAY_PIN = GBS_A3_io3_TX0; // jumper wire from pin 1 to unused A3
const byte STATUS_LED_PIN = 99; // status led not used
#else
#define FILE_NEW (O_READ | O_WRITE | O_CREAT)

#define TRIAC

const byte TONE_PIN = 2;
const byte MAIN_RELAY_PIN = 3;
const byte SD_SS_PIN = 4; // SD card SS
#ifdef TRIAC
#ifdef ARDUINO_ARCH_SAMD
const byte BYPASS_RELAY_PIN = 5;
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib
// pin 7 zero-crossing external interrupt for TriacLib
#elif defined(PROBADIO)
const byte TRIAC_PIN = 5; // TIMER1 OC1A for TriacDimmer library
// pin 6 TIMER1 ICP1 for TriacDimmer library
const byte BYPASS_RELAY_PIN = 7;
#endif
const byte ZC_EI_PIN = TRIAC_PIN + 1; // it is the second pin on the Grove connector to Dimmer
const byte BUTTON_PIN = 11;
#else
const byte BYPASS_RELAY_PIN = 5;
const byte PWM_PIN = 6;
const byte BUTTON_PIN = 7;
#endif
const byte LEDBAR_DATA_PIN = 8;
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector
const byte NET_SS_PIN = 10;
//pin 10-13 SPI (Ethernet, SD)
const byte TEMPSENS_PIN = A0;
//const byte ELSENS_PIN = A1;
const byte BALBOA_RELAY_PIN = A2;
const byte VALVES_RELAY_PIN = A3;
//pin A4, A5 is I2C on AVR (ADC, on Uno Wifi ESP8266 over I2C SC)
const byte STATUS_LED_PIN = 99; // status led not used
#endif

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
  STATS_SAVE_EVENT,
  EVENTS_SIZE
};

struct Stats {
  long heatingTime = 0; // minutes
  long consumedPower = 0; // watts
};

#endif
