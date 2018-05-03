
enum struct RestRequest {
  INDEX = 'I',
  EVENTS = 'E',
  ALARM = 'A',
  PUMP_ALARM_RESET = 'P',
  MANUAL_RUN = 'H',
  VALVES_BACK = 'V',
  SAVE_EVENTS = 'S'
};

NetServer restServer(81);

void restServerSetup() {
  restServer.begin();
}

void restServerLoop() {

  NetClient client = restServer.available();
  if (!client)
    return;
  if (client.connected()) {
    if (client.available() && client.find('/')) { // GET /fn HTTP/1.1
      char fn[15];
      int l = client.readBytesUntil(' ', fn, sizeof(fn));
      fn[l] = 0;
      while (client.read() != -1);
      char buff[150];
      if (l == 1 && strchr_P((const char*) F("IEAPHVS"), fn[0])) {
        RestRequest request = (RestRequest) fn[0];
        ChunkedPrint chunked(client, buff, sizeof(buff));
        chunked.println(F("HTTP/1.1 200 OK"));
        chunked.println(F("Content-Type: application/json"));
        chunked.println(F("Transfer-Encoding: chunked"));
        chunked.println(F("Connection: close"));
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
      } else {
        BufferedPrint bp(client, buff, sizeof(buff));
        bp.println(F("HTTP/1.1 404 Not Found"));
        bp.println(F("Connection: close"));
        bp.println();
        bp.printf(F("\"%s\" not found"), fn);
        bp.flush();
      }
    }
  }
  client.stop();
}

void printValuesJson(FormattedPrint& client) {
  client.printf(F("{\"st\":\"%c\",\"v\":\"%s\",\"r\":\"%d %d %d %d\",\"ec\":%d,\"ts\":%d"),
      state, version, mainRelayOn, bypassRelayOn, balboaRelayOn,
      valvesRelayOn, eventsRealCount(), analogRead(TEMPSENS_PIN));
  switch (state) {
    case RegulatorState::REGULATING:
    case RegulatorState::OVERHEATED:
      client.printf(F(",\"h\":%d,\"a\":%d"), heatingPower, availablePower);
      /* no break */
    case RegulatorState::MONITORING:
      client.printf(F(",\"m\":%d,\"i\":%d,\"soc\":%d,\"b\":%d"), m, inverterAC, soc, b);
      break;
    case RegulatorState::MANUAL_RUN:
      client.printf(F(",\"mr\":%u"), manualRunMinutesLeft());
      break;
    default:
      break;
  }
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

