
enum struct RestRequest {
  INDEX = 'I',
  EVENTS = 'E',
  CSV_LIST = 'L',
  ALARM = 'A',
  PUMP_ALARM_RESET = 'P',
  MANUAL_RUN = 'H',
  VALVES_BACK = 'V',
  SAVE_EVENTS = 'S'
};

NetServer webServer(80);

void webServerSetup() {
  webServer.begin();
}

void webServerLoop() {

  static NetClient client = webServer.available();

  if (!client) {
    client = webServer.available();
  }
  if (!client)
    return;
  if (client.connected()) {
    if (client.available() && client.find(' ')) { // GET /fn HTTP/1.1
      char fn[20];
      int l = client.readBytesUntil(' ', fn, sizeof(fn));
      fn[l] = 0;
      while (client.read() != -1);
      if (l == 1) {
        strcpy_P(fn, (const char*) F("/index.html"));
      }
      if (msg.length() > 0) {
        msg.print(' ');
      }
      msg.print(fn);
#ifdef ARDUINO_AVR_UNO_WIFI_DEV_ED
      char buff[64]; // some WiFi Link problem
#else
      char buff[1024];
#endif
      ChunkedPrint chunked(client, buff, sizeof(buff));
      if (l == 2 && strchr("IELAPHVS", fn[1])) {
        webServerRestRequest(fn[1], chunked);
      } else {
        webServerServeFile(fn, chunked);
      }
      client.stop();
    }
  }
}

void webServerRestRequest(char cmd, ChunkedPrint& chunked) {
  RestRequest request = (RestRequest) cmd;
  chunked.println(F("HTTP/1.1 200 OK"));
  chunked.println(F("Connection: close"));
  chunked.println(F("Content-Type: application/json"));
  chunked.println(F("Transfer-Encoding: chunked"));
  chunked.println(F("Cache-Control: no-store"));
  chunked.println(F("Access-Control-Allow-Origin: *"));
  chunked.println();
  chunked.begin();
  switch (request) {
    default:
      printValuesJson(chunked);
      break;
    case RestRequest::EVENTS:
      eventsPrintJson(chunked);
      break;
    case RestRequest::CSV_LIST:
      csvLogPrintJson(chunked);
      break;
    case RestRequest::ALARM:
      printAlarmJson(chunked);
      break;
    case RestRequest::PUMP_ALARM_RESET:
      buttonPressed = true;
      break;
    case RestRequest::MANUAL_RUN:
      manualRunRequest = true;
      break;
    case RestRequest::VALVES_BACK:
      valvesBackStart(0);
      break;
    case RestRequest::SAVE_EVENTS:
      eventsSave();
      break;
  }
  chunked.end();
}

void webServerServeFile(const char *fn, BufferedPrint& bp) {
  boolean notFound = true;
#ifdef FS
  char* ext = strchr(fn, '.');
#ifdef ESP8266
  {
    File dataFile = SPIFFS.open(fn, "r");
#else
  if (sdCardAvailable) {
    if (strlen(ext) > 4) {
      ext[4] = 0;
      memmove(ext + 2, ext, 5);
      ext[0] = '~';
      ext[1] = '1';
      ext += 2;
    }
    File dataFile = SD.open(fn);
#endif
    if (dataFile) {
      notFound = false;
      bp.println(F("HTTP/1.1 200 OK"));
      bp.println(F("Connection: close"));
      bp.print(F("Content-Length: "));
      bp.println(dataFile.size());
      bp.print(F("Content-Type: "));
      bp.println(getContentType(ext));
      if (strcmp(ext, ".CSV") == 0) {
        bp.println(F("Content-Disposition: attachment"));
      } else {
        unsigned long expires = now() + SECS_PER_YEAR;
        bp.printf(F("Expires: %s, "), dayShortStr(weekday(expires))); // two printfs because ShortStr functions share the buffer
        bp.printf(F("%d %s %d 00:00:00 GMT"), day(expires), monthShortStr(month(expires)), year(expires));
        bp.println();
      }
      bp.println();
      while (dataFile.available()) {
        bp.write(dataFile.read());
      }
      dataFile.close();
      bp.flush();
    }
  }
#endif
  if (notFound) {
    bp.println(F("HTTP/1.1 404 Not Found"));
    bp.printf(F("Content-Length: "));
    bp.println(12 + strlen(fn));
    bp.println();
    bp.printf(F("\"%s\" not found"), fn);
    bp.flush();
  }
}

void printValuesJson(FormattedPrint& client) {
  client.printf(F("{\"st\":\"%c\",\"v\":\"%s\",\"r\":\"%d %d %d %d\",\"ec\":%d,\"ts\":%d"),
      state, version, mainRelayOn, bypassRelayOn, balboaRelayOn,
      valvesRelayOn, eventsRealCount(), valvesBackTempSensRead());
  switch (state) {
    case RegulatorState::MANUAL_RUN:
      client.printf(F(",\"mr\":%u"), manualRunMinutesLeft());
      /* no break */
    case RegulatorState::REGULATING:
    case RegulatorState::OVERHEATED:
      client.printf(F(",\"h\":%d,\"a\":%d"), heatingPower, availablePower);
      /* no break */
    case RegulatorState::MONITORING:
      client.printf(F(",\"m\":%d,\"i\":%d,\"soc\":%d,\"b\":%d"), m, inverterAC, soc, b);
      break;
    default:
      break;
  }
#ifdef FS
  client.print(F(",\"csv\":1"));
#endif
  client.print('}');
}

void printAlarmJson(FormattedPrint& client) {
  client.printf(F("{\"a\":\"%c\""), (char) alarmCause);
  int eventIndex = -1;
  switch (alarmCause) {
    case AlarmCause::NOT_IN_ALARM:
      break;
    case AlarmCause::WIFI:
      eventIndex = WIFI_EVENT;
      break;
    case AlarmCause::PUMP:
      eventIndex = PUMP_EVENT;
      break;
    case AlarmCause::MODBUS:
      eventIndex = MODBUS_EVENT;
      break;
  }
  if (eventIndex != -1) {
    client.print(F(",\"e\":"));
    eventsPrintJson(client, eventIndex);
  }
  client.print('}');
}

const char* getContentType(const char* ext){
  if (!strcmp_P(ext, (const char*) F( ".html")) || !strcmp_P(ext, (const char*) F( ".htm")))
    return "text/html";
  if (!strcmp_P(ext, (const char*) F( ".css")))
    return "text/css";
  if (!strcmp_P(ext, (const char*) F( ".js")))
    return "application/javascript";
  if (!strcmp_P(ext, (const char*) F( ".png")))
    return "image/png";
  if (!strcmp_P(ext, (const char*) F( ".gif")))
    return "image/gif";
  if (!strcmp_P(ext, (const char*) F( ".jpg")))
    return "image/jpeg";
  if (!strcmp_P(ext, (const char*) F( ".ico")))
    return "image/x-icon";
  if (!strcmp_P(ext, (const char*) F( ".xml")))
    return "text/xml";
  return "text/plain";
}
