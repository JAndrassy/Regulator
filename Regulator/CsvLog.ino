
void csvLogSetup() {

  unsigned long t = now() - SECS_PER_WEEK;
  char fn0[15];
  sprintf_P(fn0, (const char*) F("%02d-%02d-%02d.CSV"), year(t) - 2000, month(t), day(t));

#ifdef __SD_H__
  File root = SD.open("/");
  File file = root.openNextFile();
  while (file) {
    const char* fn = file.name();
    const char* ext = strchr(fn, '.');
    if (strcmp(ext, ".CSV") == 0) {
      if (strcmp(fn, fn0) < 0) {
        SD.remove(fn);
      }
    }
    file.close();
    file = root.openNextFile();
  }
#else
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fn = dir.fileName();
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
    char fn[15];
    sprintf_P(fn, (const char*) F("%02d-%02d-%02d.CSV"), year(t) - 2000, month(t), day(t));
    File file = FS.open(fn, FILE_WRITE);
    if (file) {
      file.print(buff);
      file.close();
    } else {
      msg.print(F(" file error"));
    }
    lines.reset();
  }

  if (mainRelayOn) {
    lines.printf(F("%02d:%02d:%02d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d\r\n"), hour(t), minute(t), second(t),
        heatingPower, m, soc, b, availablePower, pwm, elsens, elsensPower, inverterAC, wemoPower);
  }
#endif
}

void csvLogPrintJson(FormattedPrint& out) {
#ifdef FS
  boolean first = true;
  out.print(F("{\"f\":["));
#ifdef __SD_H__
  File root = SD.open("/");
  File file;
  while (file = root.openNextFile()) { // @suppress("Assignment in condition")
#else
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    File file = dir.openFile("r");
#endif
    const char* fn = file.name();
    const char* ext = strchr(fn, '.');
    if (strcmp(ext, ".CSV") == 0) {
      char fnlc[15];
      int i = 0;
      for (; fn[i]; i++) {
        fnlc[i] = tolower(fn[i]);
      }
      fnlc[i] = 0;
      if (first) {
        first = false;
      } else {
        out.print(',');
      }
      out.printf(F("{\"fn\":\"%s\",\"size\":%ld}"), fnlc, file.size() / 1000);
    }
    file.close();
  }
  out.print(F("]}"));
#endif
}
