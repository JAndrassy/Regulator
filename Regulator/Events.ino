
const unsigned long EVENTS_SAVE_INTERVAL_SEC = 10 * 60; // sec 10 min
const char eventLabels[EVENTS_SIZE] = {'E', 'R', 'W', 'N', 'P', 'M', 'O', 'B', 'H', 'V', 'C', 'L', 'S'};
const char* eventLongLabels[EVENTS_SIZE] = {"Events", "Reset", "Watchdog", "Network", "Pump", "Modbus",
    "Overheated", "Balboa pause", "Manual run", "Valves back", "Sus.calib.", "Batt.set", "Stat.save"};

#ifdef NO_EEPROM
#define EVENTS_FILENAME "EVENTS.DAT"
#define EVENTS_LOG_FN "EVENTS.LOG"
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

const unsigned long MIN_VALID_TIME = SECS_YR_2000 + SECS_PER_YEAR;

unsigned long eventsTimer = 0;
EventStruct events[EVENTS_SIZE];

void eventsSetup() {
#ifdef NO_EEPROM
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
  if (now() > MIN_VALID_TIME) {
    if (events[RESTART_EVENT].timestamp < MIN_VALID_TIME) { // time was not set in setup()
      unsigned long t = now() - (millis() / 1000);
      events[RESTART_EVENT].timestamp = t;
      if (eventsTimer > t || eventsTimer < MIN_VALID_TIME) {
        eventsTimer = t;
      }
    }
    if (now() - eventsTimer > EVENTS_SAVE_INTERVAL_SEC) {
      eventsSave();
    }
  }
#ifdef EVENTS_LOG_FN
  if (hour(now()) == 23 && minute(now()) == 59 && FS.exists(EVENTS_LOG_FN)) {
    FS.remove(EVENTS_LOG_FN);
  }
#endif
}

void eventsWrite(int newEvent, int value1, int value2) {
  EventStruct& e = events[newEvent];
  unsigned long last = e.timestamp;
  if (now() > MIN_VALID_TIME && last > MIN_VALID_TIME  //
      && (year(last) != year() || month(last) != month() || day(last) != day())) {
    e.count = 0; // start counting with first event of this type today
  }
  e.timestamp = now();
  e.value1 = value1;
  e.value2 = value2;
  e.count++;
#ifdef EVENTS_LOG_FN
  if (newEvent != EVENTS_SAVE_EVENT) {
    File file = FS.open(EVENTS_LOG_FN, FILE_WRITE);
    if (file) {
      char buff[64];
      BufferedPrint bp(file, buff, sizeof(buff));
      eventsPrint(bp, newEvent);
      bp.println();
      bp.flush();
      file.close();
    }
  }
#endif
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
#ifdef NO_EEPROM
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
  s.printf(F("%-15s|%d-%02d-%02d %02d:%02d:%02d|% 5d|% 5d|% 3u|"), eventLongLabels[ix],
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
      sprintf(label, "%02d-%02d %02d:%02d:%02d %3d %s", month(t), day(t), hour(t), minute(t), second(t), event.count, eventLongLabels[map[i]]);
      Blynk.virtualWrite(TABLE_WIDGET, "add", i, label, event.value1 - event.value2);
    }
  }
  lastEventsUpdate = now();
}

int eventsCompare(const void * elem1, const void * elem2) {
  return (events[*((byte*) elem2)].timestamp < events[*((byte*) elem1)].timestamp) ? -1 : 1;
}

