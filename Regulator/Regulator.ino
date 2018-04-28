#include <MemoryFree.h>
#include <TimeLib.h>
#include <WiFiLink.h>
#include <UnoWiFiDevEdSerial1.h>
#include "consts.h"
#include "secrets.h"

#ifndef __IN_ECLIPSE__
#define BLYNK_NO_BUILTIN
#define BLYNK_NO_INFO
#include <BlynkSimpleWiFiLink.h>
#endif

#define NetServer WiFiServer
#define NetClient WiFiClient

unsigned long loopStartMillis;

RegulatorState state = RegulatorState::REST;
AlarmCause alarmCause = AlarmCause::NOT_IN_ALARM;

boolean buttonPressed = false;
boolean manualRunRequest = false;

boolean mainRelayOn = false;
boolean bypassRelayOn = false;
boolean valvesRelayOn = false;
boolean balboaRelayOn = false;

// SunSpec Modbus values
int b; // battery charging power
int soc; //state of charge
int m; // smart meter measured power
int inverterAC; // inverter AC power

// PowerPilot values
int availablePower;
int heatingPower;
int pwm;

//ElSens values (for logging and UI)
int elsens; // el.sensor measurement
int elsensPower; // power calculation

char msg[32];

void setup() {
  beep();
  Serial.begin(115200);
  Serial.println(version);
  Serial.print(F("mem "));
  Serial.println(freeMemory());

  Serial1.begin(115200);
  Serial1.resetESP();
  delay(3000); //ESP init
  WiFi.init(&Serial1);
  // connection is checked in loop

  pinMode(MAIN_RELAY_PIN, OUTPUT);
  pinMode(BYPASS_RELAY_PIN, OUTPUT);

  buttonSetup();
  ledBarSetup();
  valvesBackSetup();
  balboaSetup();

  telnetSetup();
  blynkSetup();
//  restServerSetup();
  modbusSetup();
  eventsSetup();
  watchdogSetup();
  battSettSetup();
  beep();
}

void loop() {

  loopStartMillis = millis();

  handleSuspendAndOff();

  watchdogLoop();
  eventsLoop();

  // clear old data to not to be show in UI
  switch (state) {
    case RegulatorState::MONITORING:
    case RegulatorState::REGULATING:
    case RegulatorState::OVERHEATED:
      // do not clear data
    break;
    default:
      modbusClearData();
      availablePower = 0;
      pwm = 0;
      elsens = 0;
      elsensPower = 0;
  }

  // user interface
  buttonPressed = false;
  buttonLoop();
  beeperLoop(); // alarm sound
  ledBarLoop();
  blynkLoop();
//  restServerLoop();
  telnetLoop(msg[0] != 0); // checks input commands and prints msg
  msg[0] = 0;  //clear msg

  if (handleAlarm())
    return;

  if (manualRunLoop())
    return;

  valvesBackLoop();

  if (restHours())
    return;

  if (!wifiConnected())
    return;

  battSettLoop();
  susCalibLoop();

  if (modbusLoop()) { // returns true if data set is ready
    balboaLoop();

    if (elsensLoop()) { // evaluates OVERHEATED
      pilotLoop();
    }
    telnetLoop(true); // logs modbus and heating data
  }
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

boolean handleAlarm() {

  if (alarmCause == AlarmCause::NOT_IN_ALARM)
    return false;
  if (state != RegulatorState::ALARM) {
    state = RegulatorState::ALARM;
    balboaReset();
    sprintfF(msg, F("alarm %c"), (char) alarmCause);
  }
  boolean stopAlarm = false;
  switch (alarmCause) {
    case AlarmCause::WIFI:
      stopAlarm = (WiFi.status() == WL_CONNECTED);
      break;
    case AlarmCause::PUMP:
      stopAlarm = buttonPressed;
      break;
    case AlarmCause::MODBUS:
      stopAlarm = requestSymoRTC();
      break;
    default:
      break;
  }
  if (stopAlarm) {
    alarmCause = AlarmCause::NOT_IN_ALARM;
  }
  return !stopAlarm;
}

boolean restHours() {

  const int BEGIN_HOUR = 9;
  const int END_HOUR = 17;

  int h = hour(now());
  if (h >= BEGIN_HOUR && h < END_HOUR)
    return false;
  if (state != RegulatorState::REST) {
    state = RegulatorState::REST;
    if (h == END_HOUR && minute(now()) == 0) {
      balboaReset();
    }
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

void t2s(unsigned long t, char *buff) {
  sprintfF(buff, F("%02d:%02d:%02d"), hour(t), minute(t), second(t));
}

void sprintfF(char* buf, const __FlashStringHelper *fmt, ... ){
   va_list args;
   va_start(args, fmt);
   vsprintf_P(buf, (const char *)fmt, args); // progmem for AVR
   va_end(args);
}

