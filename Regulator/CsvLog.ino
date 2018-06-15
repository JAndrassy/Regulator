
const char* CSV_DIR = "/CSV/";

void csvLogSetup() {

  unsigned long t = now() - SECS_PER_WEEK;
  char fn0[15];
  sprintf_P(fn0, (const char*) F("%02d-%02d-%02d.CSV"), year(t) - 2000, month(t), day(t));

#ifdef __SD_H__
  File dir = SD.open(CSV_DIR);
  File file = dir.openNextFile();
  while (file) {
    const char* fn = file.name();
    const char* ext = strchr(fn, '.');
    if (strcmp(ext, ".CSV") == 0) {
      if (strcmp(fn, fn0) < 0) {
        SD.remove(fn);
      }
    }
    file.close();
    file = dir.openNextFile();
  }
#elif ESP8266
  Dir dir = SPIFFS.openDir(CSV_DIR);
  while (dir.next()) {
    String fn = dir.fileName().substring(sizeof(CSV_DIR) + 1);
    const char* ext = strchr(fn.c_str(), '.');
    if (strcmp(ext, ".CSV") == 0) {
      if (strcmp(fn.c_str(), fn0) < 0) {
        SPIFFS.remove(fn);
      }
    }
  }
#endif
}

void csvLogLoop() {

#ifdef FS
  static char buff[2000];
  static CStringBuilder lines(buff, sizeof(buff));

  unsigned long t = now();

  if (lines.length() > sizeof(buff) - 100 || (!mainRelayOn && lines.length())) {
    char fn[20];
    sprintf_P(fn, (const char*) F("%s%02d-%02d-%02d.CSV"), CSV_DIR, year(t) - 2000, month(t), day(t));
    File file = FS.open(fn, FILE_WRITE);
    if (file) {
      file.print(buff);
      file.close();
    } else {
      msg.print(F(" file error"));
    }
    lines.reset();
  }

  if (mainRelayOn && state != RegulatorState::MANUAL_RUN) {
    lines.printf(F("%02d:%02d:%02d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d\r\n"), hour(t), minute(t), second(t),
        heatingPower, m, soc, b, availablePower, pwm, elsens, elsensPower, voltage, inverterAC, wemoPower);
  }
#endif
}

void csvLogPrintJson(FormattedPrint& out) {
#ifdef FS
  boolean first = true;
  out.print(F("{\"f\":["));
#ifdef __SD_H__
  File dir = SD.open(CSV_DIR);
  File file;
  while (file = dir.openNextFile()) { // @suppress("Assignment in condition")
    const char* fn = file.name();
#else
  Dir dir = SPIFFS.openDir(CSV_DIR);
  while (dir.next()) {
    File file = dir.openFile("r");
    const char* fn = file.name() + sizeof(CSV_DIR) + 1;
#endif
    const char* ext = strchr(fn, '.');
    if (strcmp(ext, ".CSV") == 0) {
      if (first) {
        first = false;
      } else {
        out.print(',');
      }
      out.printf(F("{\"fn\":\"%s\",\"size\":%ld}"), fn, file.size() / 1000);
    }
    file.close();
  }
  out.print(F("]}"));
#endif
}
