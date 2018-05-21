
#include <EEPROM.h>

const unsigned long EEPROM_SAVE_INTERVAL_SEC = 10 * 60; // sec 10 min
const char eventLabels[EVENTS_SIZE] = {'E', 'R', 'D', 'W', 'P', 'M', 'O', 'B', 'H', 'V', 'C', 'B'};
const int EEPROM_ADDR = 0;

struct EventStruct {
  unsigned long timestamp;
  int value1;
  int value2;
  byte count;
};

unsigned long eepromTimer = 0;
EventStruct events[EVENTS_SIZE];

void eventsSetup() {
#ifdef ESP8266
  EEPROM.begin(128);
#endif
  EEPROM.get(EEPROM_ADDR, events);
  eepromTimer = events[EEPROM_EVENT].timestamp;
  eventsWrite(RESTART_EVENT, 0, 0);
}

void eventsLoop() {
  if (now() > SECS_PER_DAY && now() - eepromTimer > EEPROM_SAVE_INTERVAL_SEC) {
    eventsSave();
  }
}

void eventsWrite(int newEvent, int value1, int value2) {
  unsigned long lastStart = events[RESTART_EVENT].timestamp;
  if (now() > SECS_PER_DAY && lastStart > SECS_PER_DAY  // to not work with 1.1.1970
      && (year(lastStart) != year() || month(lastStart) != month() || day(lastStart) != day())) {
    for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
      events[i].timestamp = 0;
      events[i].value1 = 0;
      events[i].value2 = 0;
      events[i].count = 0;
    }
    eepromTimer = 0;
  }
  EventStruct& e = events[newEvent];
  e.timestamp = now();
  e.value1 = value1;
  e.value2 = value2;
  e.count++;
}

boolean eventsSaved() {
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    if (events[i].timestamp > eepromTimer)
      return false;
  }
  return true;
}

void eventsSave() {
  if (eventsSaved())
    return;
  eventsWrite(EEPROM_EVENT, 0, 0);
  EEPROM.put(EEPROM_ADDR, events);
#ifdef ESP8266
  EEPROM.commit();
#endif
  eepromTimer = events[EEPROM_EVENT].timestamp;
  msg.print(F(" events saved"));
}

byte eventsRealCount() {
  byte ec = 0;
  for (byte  i = RESTART_EVENT; i < EVENTS_SIZE; i++) {
    ec += events[i].count;
  }
  return ec;
}

void eventsPrint(FormattedPrint& stream) {
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    EventStruct& event = events[i];
    if (i > 0) {
      if (event.timestamp == 0)
        continue;
    }
    eventsPrint(stream, i);
    stream.println();
  }
}

void eventsPrint(FormattedPrint& s, int ix) {
  unsigned long t = events[ix].timestamp;
  s.printf(F("%c|%02d:%02d:%02d|% 5d|% 5d|% 3u|"), eventLabels[ix],
      hour(t), minute(t), second(t),
      events[ix].value1, events[ix].value2, events[ix].count);
}

void eventsPrintJson(FormattedPrint& stream) {
  stream.print(F("{\"e\":["));
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    EventStruct& event = events[i];
    if (i > 0) {
      if (event.timestamp == 0)
        continue;
      stream.print(",");
    }
    eventsPrintJson(stream, i);
  }
  stream.printf(F("],\"s\":%d}"), eventsSaved());
}

void eventsPrintJson(FormattedPrint& stream, int ix) {
  stream.printf(F("{\"i\":%i,\"t\":%lu,\"v1\":%d,\"v2\":%d,\"c\":%u}"), ix, events[ix].timestamp, events[ix].value1, events[ix].value2, events[ix].count);
}
