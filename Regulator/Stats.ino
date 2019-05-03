
#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
#define STATS_FILENAME "STATS.DAT"
#else
#include <EEPROM.h>
const int STATS_EEPROM_ADDR = 256;
#endif

struct {
  time_t timestamp;

  Stats day;
  Stats month;
  Stats dayManualRun;
  Stats monthManualRun;
} statsData;

unsigned long statsMilliwats = 0;
unsigned long statsMilliwatMilis = 0;
bool statsManualRunFlag;
unsigned long statsSaveTimer = 0;

void statsSetup() {
#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
  File file = FS.open(STATS_FILENAME, FILE_READ);
  if (file) {
    file.readBytes((byte*) &statsData, sizeof(statsData));
    file.close();
  }
#else
#ifdef ESP8266
  EEPROM.begin(EEPROM_SIZE);
#endif
  EEPROM.get(STATS_EEPROM_ADDR, statsData);
#ifdef ESP8266
  EEPROM.end();
#endif
#endif
}

void statsLoop() {

  const unsigned long STATS_MILLIWATS_INTERVAL = 5 * 60000; // 5 minutes
  const unsigned long STATS_SAVE_INTERVAL = 30 * 60000; // 30 minutes
  static unsigned long lastPowerChangeMillis = 0;
  static int lastPower = 0;

  if (lastPowerChangeMillis == 0) { // not counting
    if (!mainRelayOn)
      return;
    lastPowerChangeMillis = loopStartMillis; // relay is on, start counting
    statsSaveTimer = loopStartMillis;
    statsManualRunFlag = (state == RegulatorState::MANUAL_RUN);
  }
  time_t t = statsData.timestamp;
  if (day(now()) != day(t)) {
#ifdef FS
    char buff[32];
    sprintf(buff, "DATA%d.CSV", year(t));
    File file = FS.open(buff, FILE_WRITE);
    if (file) {
      sprintf(buff, "%d-%02d-%02d;%d;%d;%d;%d", year(t), month(t), day(t), //
          statsData.day.heatingTime, statsData.day.consumedPower, //
          statsData.dayManualRun.heatingTime, statsData.dayManualRun.consumedPower);
      file.println(buff);
      file.close();
    }
#endif
    if (month(now()) != month(statsData.timestamp)) {
      memset(&statsData, 0, sizeof(statsData));
    } else {
      statsData.day.heatingTime = 0;
      statsData.day.consumedPower = 0;
      statsData.dayManualRun.heatingTime = 0;
      statsData.dayManualRun.consumedPower = 0;
    }
    msg.print("stats reset");
    statsData.timestamp = now();
  }

  unsigned long interval = loopStartMillis - lastPowerChangeMillis;
  int power = statsEvalCurrentPower();
  if (power != lastPower || interval > STATS_MILLIWATS_INTERVAL) {
    if (lastPower > 0) {
      statsMilliwats += (float) lastPower/3.6 * (float) interval/1000;
      statsMilliwatMilis += interval;
      if (power == 0 || statsMilliwatMilis > STATS_MILLIWATS_INTERVAL) {
        statsAddMilliwats();
        statsManualRunFlag = (state == RegulatorState::MANUAL_RUN);
      }
    }
    lastPower = power;
    lastPowerChangeMillis = loopStartMillis;
  }

  if (!mainRelayOn) {
    lastPowerChangeMillis = 0; // stop counting
    statsSave();
  }

  if (loopStartMillis - statsSaveTimer > STATS_SAVE_INTERVAL) {
    statsSave();
  }
}

int statsEvalCurrentPower() {
  switch (state) {
    case RegulatorState::MANUAL_RUN:
    case RegulatorState::REGULATING:
      return elsensPower;
    default:
      return 0;
  }
}

void statsAddMilliwats() {
  int heatingTime =  round((float) statsMilliwatMilis / 60000); // minutes
  int consumedPower = round((float) statsMilliwats / 1000); // watt
  statsMilliwatMilis = 0;
  statsMilliwats = 0;
  if (statsManualRunFlag) {
    statsData.dayManualRun.heatingTime += heatingTime;
    statsData.dayManualRun.consumedPower += consumedPower;
    statsData.monthManualRun.heatingTime += heatingTime;
    statsData.monthManualRun.consumedPower += consumedPower;
  } else {
    statsData.day.heatingTime += heatingTime;
    statsData.day.consumedPower += consumedPower;
    statsData.month.heatingTime += heatingTime;
    statsData.month.consumedPower += consumedPower;
  }
}

void statsSave() {
  statsAddMilliwats();
#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
  File file = FS.open(STATS_FILENAME, FILE_NEW);
  if (file) {
    file.write((byte*) &statsData, sizeof(statsData));
    file.close();
  }
#else
#ifdef ESP8266
  EEPROM.begin(EEPROM_SIZE);
#endif
  EEPROM.put(STATS_EEPROM_ADDR, statsData);
#ifdef ESP8266
  EEPROM.end();
#endif
#endif
  eventsWrite(STATS_SAVE_EVENT, (loopStartMillis - statsSaveTimer) / 60000, 0);
  statsSaveTimer = loopStartMillis;
  msg.print("stats saved");
}

int statsConsumedPowerToday() {
  if (day(now()) != day(statsData.timestamp))
    return 0;
  return statsData.day.consumedPower + statsData.dayManualRun.consumedPower;
}

void statsPrint(FormattedPrint& out) {
  statsPrint(out, "Day heating", statsData.day);
  statsPrint(out, "Month heating", statsData.month);
  statsPrint(out, "Day manual-run", statsData.dayManualRun);
  statsPrint(out, "Month manual-run", statsData.monthManualRun);
}

void statsPrint(FormattedPrint& out, const char *label, Stats &stats) {
  int h = stats.heatingTime / 60;
  int m = stats.heatingTime - h * 60;
  out.printf(F("%s time: %d:%02d\r\n"), label, h, m);
  out.printf(F("%s power: %d W\r\n"), label, stats.consumedPower);
}

void statsPrintJson(FormattedPrint& out) {
  out.printf(F("{\"timestamp\":%lu,"
      "\"dayHeatingTime\":%d,\"dayConsumedPower\":%d,"
      "\"monthHeatingTime\":%d,\"monthConsumedPower\":%d,"
      "\"dayManualRunTime\":%d,\"dayManualRunPower\":%d,"
      "\"monthManualRunTime\":%d,\"monthManualRunPower\":%d}"),
      statsData.timestamp,
      statsData.day.heatingTime, statsData.day.consumedPower,
      statsData.month.heatingTime, statsData.month.consumedPower,
      statsData.dayManualRun.heatingTime, statsData.dayManualRun.consumedPower,
      statsData.monthManualRun.heatingTime, statsData.monthManualRun.consumedPower);
}
