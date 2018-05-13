
void csvLogSetup() {

  unsigned long t = now() - SECS_PER_WEEK;
  char fn0[15];
  sprintf_P(fn0, (const char*) F("%02d-%02d-%02d.CSV"), year(t) - 2000, month(t), day(t));
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

}

void csvLogLoop() {

#ifdef __SD_H__
  static char buff[2000];
  static CStringBuilder lines(buff, sizeof(buff));

  unsigned long t = now();

  if (lines.length() > sizeof(buff) - 100 || (!mainRelayOn && lines.length())) {
    char fn[15];
    sprintf_P(fn, (const char*) F("%02d-%02d-%02d.csv"), year(t) - 2000, month(t), day(t));
    File file = SD.open(fn, FILE_WRITE);
    if (file) {
      file.print(buff);
      file.close();
    } else {
      msg.print(F(" sd error"));
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
#ifdef __SD_H__
  out.print(F("{\"f\":["));
  File root = SD.open("/");
  File file = root.openNextFile();
  boolean first = true;
  while (file) {
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
    file = root.openNextFile();
  }
  out.print(F("]}"));
#endif
}
