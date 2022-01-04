#include "arduino_secrets.h"
#include <StreamLib.h>
#include <TimeLib.h>
#include <MemoryFree.h> // https://github.com/mpflaga/Arduino-MemoryFree
//#include <WiFiNINA.h>
#include <Ethernet.h> //Ethernet 2.00 for all W5000
byte mac[] = SECRET_MAC;
#define ETHERNET
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

#define BLYNK_PRINT Serial
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
unsigned long valvesOpeningStartMillis;

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
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(BYPASS_RELAY_PIN, OUTPUT);
  digitalWrite(BYPASS_RELAY_PIN, LOW);

  pilotSetup();
  elsensSetup();
  buttonSetup();
  ledBarSetup();
//  statusLedSetup();
  balboaSetup();

  Serial.begin(115200);

  beep();

#ifdef ARDUINO_ARCH_SAMD
  rtc.begin();
  setTime(rtc.getEpoch());
#endif

  Serial.println(version);
  Serial.print(F("mem "));
  Serial.println(freeMemory());

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
    Serial.println(F("SD card initialized"));
#if defined(ARDUINO_AVR_ATMEGA1284)
    // clear the update file as soon as possible
    SDStorage.setUpdateFileName("FIRMWARE.BIN");
    SDStorage.clear(); // AVR SD bootloaders don't delete the update file
#endif
  }
#endif

  IPAddress ip(192, 168, 1, 6);
#ifdef ETHERNET
  Ethernet.init(NET_SS_PIN);
  Ethernet.begin(mac, ip);
  delay(500);
#else
  WiFi.config(ip);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  WiFi.setFeedWatchdogFunc([]() {watchdogLoop();});
#endif
  // connection is checked in loop

  ArduinoOTA.beforeApply(shutdown);
#if defined(ARDUINO_AVR_ATMEGA1284) // app binary is larger than half of the flash
  ArduinoOTA.begin(ip, "regulator", "password", SDStorage);
#else
  ArduinoOTA.begin(ip, "regulator", "password", InternalStorage);
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

  handleRelaysOnStates();
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
  watchdogLoop();
}

void handleRelaysOnStates() {

  static unsigned long lastOn = 0; // millis for the cool-down timer

  if (state == RegulatorState::OPENING_VALVES && loopStartMillis - valvesOpeningStartMillis > VALVE_ROTATION_TIME) {
    state = RegulatorState::MONITORING;
    turnPumpRelayOn();
  }
  if (state != RegulatorState::REGULATING && state != RegulatorState::MANUAL_RUN) {
    heatingPower = 0;
  }
  if (heatingPower > 0 || state == RegulatorState::OPENING_VALVES) {
    lastOn = loopStartMillis;
  } else if (mainRelayOn) { // && heatingPower == 0
    powerPilotStop();
    if (bypassRelayOn) {
      msg.print(F(" BR_off"));
      waitZeroCrossing();
      digitalWrite(BYPASS_RELAY_PIN, LOW);
      bypassRelayOn = false;
    }
    if (loopStartMillis - lastOn > PUMP_STOP_MILLIS) {
      msg.print(F(" MR_off"));
      waitZeroCrossing();
      digitalWrite(PUMP_RELAY_PIN, LOW);
      waitZeroCrossing();
      delay(5);
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
  if (state == RegulatorState::OPENING_VALVES)
    return false;
  if (mainRelayOn)
    return true;
  bool valvesAreOpen = !valvesBackExecuted(); // before valvesBackReset()
  valvesBackReset();
  msg.print(F(" MR_on"));
  digitalWrite(MAIN_RELAY_PIN, HIGH);
  mainRelayOn = true;
  if (valvesAreOpen)
    return turnPumpRelayOn();
  state = RegulatorState::OPENING_VALVES;
  valvesOpeningStartMillis = loopStartMillis;
  return false;
}

bool turnPumpRelayOn() {
  if (!mainRelayOn) {
    msg.print(F(" PR_err"));
    return false;
  }
  msg.print(F(" PR_on"));
  waitZeroCrossing();
  digitalWrite(PUMP_RELAY_PIN, HIGH);
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
