
const unsigned long EVENTS_SAVE_INTERVAL_SEC = 10 * 60; // sec 10 min
const char eventLabels[EVENTS_SIZE] = {'E', 'R', 'W', 'N', 'P', 'M', 'O', 'B', 'H', 'V', 'C', 'L', 'S'};
const char* eventLongLabels[EVENTS_SIZE] = {"Events", "Reset", "Watchdog", "Network", "Pump", "Modbus",
    "Overheated", "Balboa pause", "Manual run", "Valves back", "Sus.calib.", "Batt.set", "Stat.save"};

#ifdef ARDUINO_ARCH_SAMD
#define EVENTS_FILENAME "EVENTS.DAT"
#else
#include <EEPROM.h>
const int EVENTS_EEPROM_ADDR = 0;
#endif

struct EventStruct {
  unsigned long timestamp;
  int value1;
  int value2;
  byte count;
};

unsigned long eventsTimer = 0;
EventStruct events[EVENTS_SIZE];

void eventsSetup() {
#ifdef ARDUINO_ARCH_SAMD
  if (!FS.exists(EVENTS_FILENAME)) {
    for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
      events[i].timestamp = 0;
    }
  } else {
    File file = FS.open(EVENTS_FILENAME, FILE_READ);
    file.readBytes((char*) events, sizeof(events));
    file.close();
  }
#else
#ifdef ESP8266
  EEPROM.begin(EEPROM_SIZE);
#endif
  EEPROM.get(EVENTS_EEPROM_ADDR, events);
#ifdef ESP8266
  EEPROM.end();
#endif
#endif
  eventsTimer = events[EVENTS_SAVE_EVENT].timestamp;
  eventsWrite(RESTART_EVENT, 0, 0);
}

void eventsLoop() {
  if (now() > SECS_PER_DAY && now() - eventsTimer > EVENTS_SAVE_INTERVAL_SEC) {
    eventsSave();
  }
}

void eventsWrite(int newEvent, int value1, int value2) {
  EventStruct& e = events[newEvent];
  unsigned long last = e.timestamp;
  if (now() > SECS_PER_DAY && last > SECS_PER_DAY  // to not work with 1.1.1970
      && (year(last) != year() || month(last) != month() || day(last) != day())) {
    e.count = 0; // start counting with first event of this type today
  }
  e.timestamp = now();
  e.value1 = value1;
  e.value2 = value2;
  e.count++;
}

boolean eventsSaved() {
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    if (events[i].timestamp > eventsTimer)
      return false;
  }
  return true;
}

void eventsSave() {
  if (eventsSaved())
    return;
  eventsWrite(EVENTS_SAVE_EVENT, 0, 0);
#ifdef ARDUINO_ARCH_SAMD
  File file = FS.open(EVENTS_FILENAME, FILE_NEW);
  if (file) {
    file.write((byte*) events, sizeof(events));
    file.close();
  }
#else
#ifdef ESP8266
  EEPROM.begin(EEPROM_SIZE);
#endif
  EEPROM.put(EVENTS_EEPROM_ADDR, events);
#ifdef ESP8266
  EEPROM.end();
#endif
#endif
  eventsTimer = events[EVENTS_SAVE_EVENT].timestamp;
  msg.print(F(" events saved"));
}

byte eventsRealCount() {
  byte ec = 0;
  unsigned long midnight = previousMidnight(now());
  for (byte  i = RESTART_EVENT; i < STATS_SAVE_EVENT; i++) {
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

