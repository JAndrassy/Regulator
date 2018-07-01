
#include <EEPROM.h>

const unsigned long EEPROM_SAVE_INTERVAL_SEC = 10 * 60; // sec 10 min
const char eventLabels[EVENTS_SIZE] = {'E', 'R', 'D', 'W', 'P', 'M', 'O', 'B', 'H', 'V', 'C', 'L'};
const char* eventLongLabels[EVENTS_SIZE] = {"EEPROM", "Reset", "Watchdog", "WiFi", "Pump", "Modbus",
    "Overheated", "Balboa pause", "Manual run", "Valves back", "Sus.calib.", "Batt.set"};
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
  EEPROM.begin(256);
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
  EventStruct& e = events[newEvent];
  unsigned long last = e.timestamp;
  if (now() > SECS_PER_DAY && last > SECS_PER_DAY  // to not work with 1.1.1970
      && (year(last) != year() || month(last) != month() || day(last) != day())) {
    e.count = 0;
  }
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
  unsigned long midnight = previousMidnight(now());
  for (byte  i = RESTART_EVENT; i < EVENTS_SIZE; i++) {
    if (events[i].timestamp > midnight) {
      ec += events[i].count;
    }
  }
  return ec;
}

void eventsPrint(FormattedPrint& stream) {
  byte map[EVENTS_SIZE];
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    map[i] = i;
  }
  qsort(map, EVENTS_SIZE, 1, eventsCompare);
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    EventStruct& event = events[map[i]];
    if (i > 0) {
      if (event.timestamp == 0)
        continue;
    }
    eventsPrint(stream, map[i]);
    stream.println();
  }
}

void eventsPrint(FormattedPrint& s, int ix) {
  unsigned long t = events[ix].timestamp;
  s.printf(F("%c|%d-%02d-%02d %02d:%02d:%02d|% 5d|% 5d|% 3u|"), eventLabels[ix],
      year(t), month(t), day(t), hour(t), minute(t), second(t),
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

void eventsBlynk() {
  static unsigned long lastEventsUpdate = 0;

  unsigned int i = 0;
  for (; i < EVENTS_SIZE; i++) {
    if (lastEventsUpdate < events[i].timestamp)
      break;
  }
  if (i == EVENTS_SIZE)
    return;

  Blynk.virtualWrite(TABLE_WIDGET, "clr");
  byte map[EVENTS_SIZE];
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    map[i] = i;
  }
  qsort(map, EVENTS_SIZE, 1, eventsCompare);
  for (unsigned int i = 0; i < EVENTS_SIZE - 1; i++) {
    EventStruct& event = events[map[i]];
    unsigned long t = event.timestamp;
    if (t != 0) {
      char label[32];
      sprintf(label, "%02d. %02d:%02d:%02d %s", day(t), hour(t), minute(t), second(t), eventLongLabels[map[i]]);
      Blynk.virtualWrite(TABLE_WIDGET, "add", i, label, event.count == 1 ? (event.value1 + event.value2) : event.count);
    }
  }
  lastEventsUpdate = now();
}

int eventsCompare(const void * elem1, const void * elem2) {
  return events[*((byte*) elem2)].timestamp - events[*((byte*) elem1)].timestamp;
}

