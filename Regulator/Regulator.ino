#include "arduino_secrets.h"
#include <StreamLib.h>
#include <TimeLib.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#define FS SPIFFS
#define freeMemory ESP.getFreeHeap
#define beforeApply onStart
#elif ARDUINO_AVR_UNO_WIFI_DEV_ED
#include <MemoryFree.h>
#include <WiFiLink.h>
#include <UnoWiFiDevEdSerial1.h>
#else
#include <MemoryFree.h> // https://github.com/mpflaga/Arduino-MemoryFree
#include <Ethernet.h> //Ethernet 2.00 for all W5000
//#include <UIPEthernet.h> // for ENC28j60
byte mac[] = SECRET_MAC;
#define ETHERNET
#include <SD.h>
#define FS SD
#define NO_OTA_PORT
#include <ArduinoOTA.h>
#endif

#ifdef ARDUINO_SAM_ZERO
#define Serial SerialUSB
#endif

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
#define NO_EEPROM
#else
#include <EEPROM.h>
#endif

#include "consts.h"
#ifdef TRIAC
#include <TriacLib.h>
#endif

#define BLYNK_PRINT Serial
#define BLYNK_NO_BUILTIN // Blynk doesn't handle pins
#ifdef ESP8266
#include <BlynkSimpleEsp8266.h>
#elif defined(ethernet_h_)
#include <BlynkSimpleEthernet.h>
#elif defined (UIPCLIENT_H)
#include <BlynkSimpleUIPEthernet.h>
#elif defined(ARDUINO_AVR_UNO_WIFI_DEV_ED)
#define BLYNK_NO_INFO
#include <BlynkSimpleWiFiLink.h>
#endif

#ifdef ARDUINO_ARCH_SAMD
#include <RTCZero.h>
RTCZero rtc;
#endif

#ifdef ETHERNET
#define NetServer EthernetServer
#define NetClient EthernetClient
#else
#define NetServer WiFiServer
#define NetClient WiFiClient
#endif

unsigned long loopStartMillis;
byte hourNow; // current hour

RegulatorState state = RegulatorState::MONITORING;
AlarmCause alarmCause = AlarmCause::NOT_IN_ALARM;

boolean buttonPressed = false;

boolean mainRelayOn = false;
boolean bypassRelayOn = false;
boolean valvesRelayOn = false;
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

char msgBuff[32];
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
  digitalWrite(BYPASS_RELAY_PIN, LOW);

  pilotSetup();
  valvesBackSetup();
  elsensSetup();
  buttonSetup();
  ledBarSetup();
//  statusLedSetup();
  balboaSetup();

#ifndef ESP8266
  Serial.begin(115200); // TX can be used if Serial is not used
#endif

  beep();

#ifdef ARDUINO_ARCH_SAMD
  rtc.begin();
  setTime(rtc.getEpoch());
#endif

  Serial.println(version);
  Serial.print(F("mem "));
  Serial.println(freeMemory());

#ifdef __SD_H__
  pinMode(NET_SS_PIN, OUTPUT);
  digitalWrite(NET_SS_PIN, HIGH); // unselect network device on SPI bus
  if (SD.begin(SD_SS_PIN)) {
    SdFile::dateTimeCallback(sdTimeCallback);
    sdCardAvailable = true;
    Serial.println(F("SD card initialized"));
  }
#endif

  IPAddress ip(192, 168, 1, 6);
  IPAddress gw(192, 168, 1, 1);
  IPAddress sn(255, 255, 255, 0);
#ifdef ETHERNET
  Ethernet.init(NET_SS_PIN);
  Ethernet.begin(mac, ip, gw, gw, sn);
#elif defined(ESP8266)
  WiFi.hostname("regulator");
  SPIFFS.begin();
  WiFi.config(ip, gw, sn, gw);
  WiFi.begin();
  WiFi.waitForConnectResult();
#else
  Serial1.begin(115200);
  Serial1.resetESP();
  delay(3000); //ESP init
  WiFi.init(&Serial1);
  ip = WiFi.localIP();
#endif
  // connection is checked in loop

  ArduinoOTA.beforeApply(shutdown);
#if defined(ESP8266)
  MDNS.begin("regulator");
  ArduinoOTA.begin();
#elif defined(ARDUINO_AVR_ATMEGA1284) // app binary is larger then half of the flash
  SDStorage.setUpdateFileName("FIRMWARE.BIN");
  SDStorage.clear(); // AVR SD bootloaders don't delete the update file
  ArduinoOTA.begin(ip, "regulator", "password", SDStorage);
#else
  ArduinoOTA.begin(ip, "regulator", "password", InternalStorage);
#endif

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
  analogWriteResolution(10);
#endif

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
  hourNow = hour(now());

  handleSuspendAndOff();
  statsLoop();

  ArduinoOTA.handle();
  watchdogLoop();
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

  susCalibLoop();

  elsensLoop();
//  wemoLoop();
  consumptionMeterLoop();

  pilotLoop();

  battSettLoop();
  balboaLoop();

  telnetLoop(true); // logs modbus and heating data
  csvLogLoop();
  blynkChartData();
}

void shutdown() {
  eventsSave();
  statsSave();
  watchdogLoop();
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
      waitZeroCrossing();
      digitalWrite(MAIN_RELAY_PIN, LOW);
      mainRelayOn = false;
    }
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

  const int BEGIN_HOUR = 8;
  const int END_HOUR = 22; // to monitor discharge

  if (balboaRelayOn)
    return false;

  if (hourNow >= BEGIN_HOUR && hourNow < END_HOUR) {
    if (state == RegulatorState::REST) {
      requestSymoRTC(); // every morning sync clock
      if (hour() < BEGIN_HOUR)  // sync can set the clock back, then would the powerPlan reset
        return true;
      state = RegulatorState::MONITORING;
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
#ifdef TRIAC
  Triac::waitZeroCrossing();
#else
  elsensWaitZeroCrossing();
#endif
}
