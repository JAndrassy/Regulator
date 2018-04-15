
#include <EEPROM.h>

const unsigned long EEPROM_SAVE_INTERVAL_SEC = 10 * 60; // sec 10 min
const char eventLabels[EVENTS_SIZE] = {'E', 'R', 'D', 'W', 'P', 'M', 'O', 'B', 'H', 'V', 'C'};
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
  eepromTimer = events[EEPROM_EVENT].timestamp;
  sprintfF(msg, F("events saved"));
}

byte eventsRealCount() {
  byte ec = 0;
  for (byte  i = RESTART_EVENT; i < EVENTS_SIZE; i++) {
    ec += events[i].count;
  }
  return ec;
}

void eventsPrint(Print& stream) {
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

void eventsPrint(Print& s, int ix) {
  char tempBuff[50];
  sprintfF(tempBuff, F("%c|00:00:00|% 5d|% 5d|% 3u|"), eventLabels[ix], events[ix].value1, events[ix].value2, events[ix].count);
  t2s(events[ix].timestamp, tempBuff+2);
  tempBuff[10] = '|';
  s.print(tempBuff);
}

void eventsPrintJson(Print& stream) {
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
  stream.print(F("],\"s\":"));
  stream.print(eventsSaved());
  stream.print('}');
}

void eventsPrintJson(Print& stream, int ix) {
  char buff[50];
  sprintfF(buff, F("{\"i\":%i,\"t\":%lu,\"v1\":%d,\"v2\":%d,\"c\":%u}"), ix, events[ix].timestamp, events[ix].value1, events[ix].value2, events[ix].count);
  stream.print(buff);
}
