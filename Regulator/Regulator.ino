#include <StreamLib.h>
#include <TimeLib.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#define FS SPIFFS
#define freeMemory ESP.getFreeHeap
#else
#include <MemoryFree.h>
#ifdef ARDUINO_AVR_UNO_WIFI_DEV_ED
#include <WiFiLink.h>
#include <UnoWiFiDevEdSerial1.h>
#else
#include <Ethernet2.h>
#include <SD.h>
#define FS SD
#endif
#endif
#include "consts.h"

#define BLYNK_PRINT Serial
#define BLYNK_NO_BUILTIN // Blynk doesn't handle pins
#ifdef ESP8266
#include <BlynkSimpleEsp8266.h>
#elif defined(ethernet_h)
#include <BlynkSimpleEthernet2.h>
#else
#define BLYNK_NO_INFO
#include <BlynkSimpleWiFiLink.h>
#endif

#ifdef ethernet_h
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
boolean manualRunRequest = false;

boolean mainRelayOn = false;
boolean bypassRelayOn = false;
boolean valvesRelayOn = false;
boolean balboaRelayOn = false;

int freeMem;

// SunSpec Modbus values
int b; // battery charging power
int soc; //state of charge
int m; // smart meter measured power
int voltage; // inverter measured AC voltage
int inverterAC; // inverter AC power

// PowerPilot values
int availablePower;
int heatingPower;
int pwm;

//ElSens values (for logging and UI)
int elsens; // el.sensor measurement
int elsensPower; // power calculation

//Wemo Insight measurement
int wemoPower;

char msgBuff[32];
CStringBuilder msg(msgBuff, sizeof(msgBuff));

boolean sdCardAvailable = false;

void setup() {
  beep();

  Serial.begin(115200);
  Serial.println(version);
  Serial.print(F("mem "));
  Serial.println(freeMemory());

#ifdef ESP8266
#ifdef FS
  SPIFFS.begin();
#endif
  MDNS.begin("regulator");
  ArduinoOTA.begin();
  IPAddress ip(192, 168, 1, 8);
  IPAddress gw(192, 168, 1, 1);
  IPAddress sn(255, 255, 255, 0);
  WiFi.config(ip, gw, sn, gw);
  WiFi.waitForConnectResult();
#elif defined(ethernet_h)
  IPAddress ip(192, 168, 1, 8);
  Ethernet.begin(mac, ip);
#else
  Serial1.begin(115200);
  Serial1.resetESP();
  delay(3000); //ESP init
  WiFi.init(&Serial1);
  // connection is checked in loop
#endif

#ifdef __SD_H__
  if (SD.begin(SD_SS_PIN)) {
    sdCardAvailable = true;
    Serial.println(F("SD card initialized"));
  }
#endif

  pinMode(MAIN_RELAY_PIN, OUTPUT);
  pinMode(BYPASS_RELAY_PIN, OUTPUT);

  buttonSetup();
  ledBarSetup();
  valvesBackSetup();
  balboaSetup();

  telnetSetup();
  blynkSetup();
  webServerSetup();
  modbusSetup();
  eventsSetup();
  csvLogSetup();
  watchdogSetup();
  beep();
}

void loop() {

  freeMem = freeMemory();
  loopStartMillis = millis();
  hourNow = hour(now());

  handleSuspendAndOff();

#ifdef ESP8266
  ArduinoOTA.handle();
#endif
  watchdogLoop();
  eventsLoop();

  // user interface
  buttonPressed = false;
  buttonLoop();
  beeperLoop(); // alarm sound
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

#ifndef ethernet_h
  if (!wifiConnected())
    return;
#endif

  susCalibLoop();

  if (!modbusLoop()) // returns true if data-set is ready
    return;

  elsensLoop();
  wemoLoop();

  pilotLoop();

  battSettLoop();
  balboaLoop();

  telnetLoop(true); // logs modbus and heating data
  csvLogLoop();
}

void handleSuspendAndOff() {

  static unsigned long lastOn = 0; // millis for the cool-down timer

  if (state != RegulatorState::REGULATING && state != RegulatorState::MANUAL_RUN) {
    heatingPower = 0;
  }
  if (heatingPower > 0) {
    lastOn = loopStartMillis;
  } else if (mainRelayOn) { // && heatingPower == 0
    analogWrite(PWM_PIN, 0);
    pwm = 0;
    if (bypassRelayOn) {
      digitalWrite(BYPASS_RELAY_PIN, LOW);
      bypassRelayOn = false;
    }
    if (loopStartMillis - lastOn > PUMP_STOP_MILLIS) {
      digitalWrite(MAIN_RELAY_PIN, LOW);
      mainRelayOn = false;
    }
  }
}

void clearData() {
  modbusClearData();
  availablePower = 0;
  pwm = 0;
  elsens = 0;
  elsensPower = 0;
  wemoPower = 0;
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
#ifndef ethernet_h
    case AlarmCause::WIFI:
      stopAlarm = (WiFi.status() == WL_CONNECTED);
      break;
#endif
    case AlarmCause::PUMP:
      stopAlarm = buttonPressed;
      break;
    case AlarmCause::MODBUS:
      if (millis() - modbusCheckMillis > MODBUS_CHECK_INTERVAL) {
        stopAlarm = requestSymoRTC();
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

  const int BEGIN_HOUR = 9;
  const int END_HOUR = 22; // to monitor discharge

  if (balboaRelayOn)
    return false;

  if (hourNow >= BEGIN_HOUR && hourNow < END_HOUR) {
    if (state == RegulatorState::REST) {
      state = RegulatorState::MONITORING;
    }
    return false;
  }
  if (state == RegulatorState::MONITORING) {
    state = RegulatorState::REST;
    clearData();
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

#ifndef ethernet_h
boolean wifiConnected() {
  static int tryCount = 0;
  if (WiFi.status() == WL_CONNECTED) {
    tryCount = 0;
    return true;
  }
  tryCount++;
  if (tryCount == 30) {
    alarmCause = AlarmCause::WIFI;
    eventsWrite(WIFI_EVENT, WiFi.status(), 0);
  }
  return false;
}
#endif
