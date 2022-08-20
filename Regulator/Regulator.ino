#include "arduino_secrets.h"
#include <StreamLib.h>
#include <TimeLib.h>
#include <MemoryFree.h> // https://github.com/mpflaga/Arduino-MemoryFree
//#include <WiFiNINA.h>
//#include <WiFi101.h>
#include <WiFiEspAT.h> // with https://github.com/JiriBilek/ESP_ATMod for more than 1 server
//#include <Ethernet.h> //Ethernet 2.00 for all W5000
//byte mac[] = SECRET_MAC;
//#define ETHERNET
#include <SD.h>
#define FS SD
#define NO_OTA_PORT
#include <ArduinoOTA.h>

#if defined(ARDUINO_ARCH_SAMD)
#define NO_EEPROM
#else
#include <EEPROM.h>
#endif

#include <TriacLib.h>

#if defined(ARDUINO_AVR_ATMEGA1284)
#define SERIAL_AT Serial
#else
#define SERIAL_AT Serial1
#endif
//#define SERIAL_DEBUG Serial

//#define BLYNK_PRINT SERIAL_DEBUG
#define BLYNK_NO_BUILTIN // Blynk doesn't handle pins
#define BLYNK_MAX_SENDBYTES 256
#define BLYNK_USE_128_VPINS
#ifdef ETHERNET
#include <BlynkSimpleEthernet.h>
#else
#include <BlynkSimpleWifi.h>
#endif

#ifdef ARDUINO_ARCH_SAMD
#include <RTCZero.h>
RTCZero rtc;
#endif

#include "consts.h"

#ifdef ETHERNET
#define NetServer EthernetServer
#define NetClient EthernetClient
#else
#define NetServer WiFiServer
#define NetClient WiFiClient
#endif

unsigned long loopStartMillis;
byte hourNow; // current hour

const char* LOG_FN = "EVENTS.LOG";

RegulatorState state = RegulatorState::MONITORING;
AlarmCause alarmCause = AlarmCause::NOT_IN_ALARM;

boolean buttonPressed = false;
boolean manualRunRequest = false; // manual run start or stop request

boolean mainRelayOn = false;
boolean bypassRelayOn = false;
boolean valvesBackRelayOn = false;
boolean balboaRelayOn = false;

int freeMem;

// SunSpec Modbus values
int pvChargingPower; // battery charging power
int pvSOC; //state of charge
bool pvBattCalib; // true if Symo calibrates the battery
int meterPower; // smart meter measured power
int meterPowerPhaseA;
int meterPowerPhaseB;
int meterPowerPhaseC;
int voltage; // smart meter measured AC voltage
int inverterAC; // inverter AC power

// PowerPilot values
byte powerPilotPlan = BATTERY_PRIORITY;
int heatingPower;
int powerPilotRaw;

//ElSens values (for logging and UI)
int elsens; // el.sensor measurement
int elsensPower; // power calculation

//external meter measurement
int measuredPower;

// additional heater control over Wemo Inside smart socket
byte extHeaterPlan = EXT_HEATER_NORMAL;
bool extHeaterIsOn = true; // assume is on to turn it off in loop

char msgBuff[256];
CStringBuilder msg(msgBuff, sizeof(msgBuff));

bool sdCardAvailable = false;

#ifdef __SD_H__
void sdTimeCallback(uint16_t* date, uint16_t* time) {
  *date = FAT_DATE(year(), month(), day());
  *time = FAT_TIME(hour(), minute(), second());
}
#endif

void setup() {
  pinMode(MAIN_RELAY_PIN, OUTPUT);
  pinMode(BYPASS_RELAY_PIN, OUTPUT);

  pilotSetup();
  elsensSetup();
  buttonSetup();
  ledBarSetup();
//  statusLedSetup();
  balboaSetup();

  beep();

#ifdef ARDUINO_ARCH_SAMD
  rtc.begin();
  setTime(rtc.getEpoch());
#endif

#ifdef SERIAL_DEBUG
  SERIAL_DEBUG.begin(115200);
  SERIAL_DEBUG.println(version);
  SERIAL_DEBUG.print(F("mem "));
  SERIAL_DEBUG.println(freeMemory());
#endif

#ifdef __SD_H__
#ifdef NET_SS_PIN
  pinMode(NET_SS_PIN, OUTPUT);
  digitalWrite(NET_SS_PIN, HIGH); // unselect network device on SPI bus
#endif
  if (!SD.begin(SD_SS_PIN)) {
    alarmSound();
  } else {
    SdFile::dateTimeCallback(sdTimeCallback);
    sdCardAvailable = true;
#ifdef SERIAL_DEBUG
    SERIAL_DEBUG.println(F("SD card initialized"));
#endif
#if defined(ARDUINO_AVR_ATMEGA1284)
    // clear the update file as soon as possible
    SDStorage.setUpdateFileName("FIRMWARE.BIN");
    SDStorage.clear(); // AVR SD bootloaders don't delete the update file
#endif
  }
#endif

#ifdef ETHERNET
  IPAddress ip(192, 168, 1, 6);
  Ethernet.init(NET_SS_PIN);
  Ethernet.begin(mac, ip);
  delay(500);
#elif defined(_WIFI_ESP_AT_H_)
  SERIAL_AT.begin(250000);
  SERIAL_AT.setTimeout(2000);
  WiFi.init(SERIAL_AT);
  WiFi.setHostname("regulator");
  delay(2000); // the AP is remembered in the firmware and start automatically
#else
  WiFi.setHostname("regulator");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
#endif
  // connection is checked in loop

  ArduinoOTA.onStart([]() {watchdogStop();});
  ArduinoOTA.beforeApply(shutdown);
#if defined(ARDUINO_AVR_ATMEGA1284) // app binary is larger than half of the flash
  ArduinoOTA.begin(INADDR_NONE, "regulator", "password", SDStorage);
#else
  ArduinoOTA.begin(INADDR_NONE, "regulator", "password", InternalStorage);
#endif

  msg.print(F("start"));

  beep();

  valvesBackSetup();
  telnetSetup();
  blynkSetup();
  webServerSetup();
  modbusSetup();
  eventsSetup();
  statsSetup();
  csvLogSetup();
  watchdogSetup();

  beep();
}

void loop() {

  freeMem = freeMemory();
  loopStartMillis = millis();
  hourNow = hour();

  handleSuspendAndOff();
  statsLoop();

  watchdogLoop();
  ArduinoOTA.handle();
  eventsLoop();

  // user interface
  buttonPressed = false;
  buttonLoop();
  beeperLoop(); // alarm sound
//  statusLedLopp();
  ledBarLoop();
  blynkLoop();
  webServerLoop();
  telnetLoop(msg.length() != 0); // checks input commands and prints msg
  if (msg.length()) {
    log(msgBuff);
  }
  msg.reset();  //clear msg

  if (handleAlarm())
    return;

  manualRunLoop();

  valvesBackLoop();

  if (restHours())
    return;

  if (!networkConnected())
    return;

  if (!modbusLoop()) // returns true if data-set is ready
    return;

  elsensLoop();

  extHeaterLoop();
  pilotLoop();

//  wemoLoop();
  consumptionMeterLoop();
  battSettLoop();
  balboaLoop();

  telnetLoop(true); // logs modbus and heating data
  csvLogLoop();
  blynkChartData();

  susCalibLoop();
}

void shutdown() {
#ifdef UIP_PERIODIC_TIMER
  for (int i = 0; i < 4; i++) {
    Ethernet.maintain();
    delay(UIP_PERIODIC_TIMER);
  }
#endif
  eventsSave();
  statsSave();
}

void handleSuspendAndOff() {

  static unsigned long lastOn = 0; // millis for the cool-down timer

  if (state != RegulatorState::REGULATING && state != RegulatorState::MANUAL_RUN) {
    heatingPower = 0;
  }
  if (heatingPower > 0) {
    lastOn = loopStartMillis;
  } else if (mainRelayOn) { // && heatingPower == 0
    powerPilotStop();
    if (bypassRelayOn) {
      waitZeroCrossing();
      digitalWrite(BYPASS_RELAY_PIN, LOW);
      bypassRelayOn = false;
    }
    if (loopStartMillis - lastOn > PUMP_STOP_MILLIS) {
      msg.print(F(" MR_off"));
      digitalWrite(MAIN_RELAY_PIN, LOW);
      mainRelayOn = false;
    }
  }
  if (extHeaterIsOn //
      && ((state != RegulatorState::REGULATING && state != RegulatorState::OVERHEATED) || extHeaterPlan == EXT_HEATER_DISABLED) //
      && networkConnected()) {
    extHeaterStop();
  }
}

void clearData() {
  modbusClearData();
  powerPilotRaw = 0;
  elsens = 0;
  elsensPower = 0;
  measuredPower = 0;
}

boolean handleAlarm() {

  static unsigned long modbusCheckMillis;
  const unsigned long MODBUS_CHECK_INTERVAL = 5000;

  if (alarmCause == AlarmCause::NOT_IN_ALARM)
    return false;
  if (state != RegulatorState::ALARM) {
    state = RegulatorState::ALARM;
    clearData();
    balboaReset();
    msg.printf(F("alarm %c"), (char) alarmCause);
  }
  boolean stopAlarm = false;
  switch (alarmCause) {
    case AlarmCause::NETWORK:
#ifdef ETHERNET
      stopAlarm = (Ethernet.linkStatus() != LinkOFF);
#else
#ifndef _WIFI_ESP_AT_H_ // AT fw attempts reconnect by itself
      WiFi.begin(SECRET_SSID, SECRET_PASS); // for WiFiNINA and WiFi101
#endif
      stopAlarm = (WiFi.status() == WL_CONNECTED);
#endif
      break;
    case AlarmCause::PUMP:
      stopAlarm = buttonPressed;
      break;
    case AlarmCause::MODBUS:
      if (millis() - modbusCheckMillis > MODBUS_CHECK_INTERVAL) {
        stopAlarm = requestSymoRTC() && requestMeter(); // meter is off in Symo emergency power mode
        modbusCheckMillis = millis();
      }
      break;
    default:
      break;
  }
  if (stopAlarm) {
    alarmCause = AlarmCause::NOT_IN_ALARM;
    state = RegulatorState::MONITORING;
  }
  return !stopAlarm;
}

boolean restHours() {

  const unsigned long MIN_VALID_TIME = SECS_YR_2000 + SECS_PER_YEAR;
  const int BEGIN_HOUR = 8;
  const int END_HOUR = 22; // to monitor discharge

  if (now() < MIN_VALID_TIME || balboaRelayOn)
    return false;

  if (hourNow >= BEGIN_HOUR && hourNow < END_HOUR) {
    if (state == RegulatorState::REST) {
      requestSymoRTC(); // every morning sync clock
      if (hour() < BEGIN_HOUR)  // sync can set the clock back, then would the powerPlan reset
        return true;
      state = RegulatorState::MONITORING;
#ifdef FS
      FS.remove(LOG_FN);
#endif
    }
    return false;
  }
  if (state == RegulatorState::MONITORING) {
    state = RegulatorState::REST;
    clearData();
    powerPilotSetPlan(BATTERY_PRIORITY);
  }
  return true;
}

boolean turnMainRelayOn() {
  if (mainRelayOn)
    return true;
  valvesBackReset();
  msg.print(F(" MR_on"));
  digitalWrite(MAIN_RELAY_PIN, HIGH);
  mainRelayOn = true;
  return elsensCheckPump();
}

boolean networkConnected() {
  static int tryCount = 0;
#ifdef ETHERNET
  if (Ethernet.linkStatus() != LinkOFF) {
#else
  if (WiFi.status() == WL_CONNECTED) {
#endif
    tryCount = 0;
    return true;
  }
  tryCount++;
  if (tryCount == 30) {
    alarmCause = AlarmCause::NETWORK;
#ifdef ETHERNET
    eventsWrite(NETWORK_EVENT, Ethernet.linkStatus(), 0);
#else
    eventsWrite(NETWORK_EVENT, WiFi.status(), 0);
#endif
  }
  return false;
}

void waitZeroCrossing() {
  Triac::waitZeroCrossing();
}

void log(const char *msg) {
#ifdef FS
  File file = FS.open(LOG_FN, FILE_WRITE);
  if (file) {
    unsigned long t = now();
    char buff[10];
    sprintf_P(buff, PSTR("%02d:%02d:%02d "), hour(t), minute(t), second(t));
    file.print(buff);
    file.println(msg);
    file.close();
  }
#endif
}
