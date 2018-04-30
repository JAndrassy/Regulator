
NetServer restServer(81);

void restServerSetup() {
  restServer.begin();
}

void restServerLoop() {
  enum {
    RS_INDEX,
    RS_EVENTS,
    RS_ALARM,
    RS_ALARM_RESET,
    RS_MANUAL_RUN,
    RS_VALVES_BACK,
    RS_SAVE_EVENTS
  };

  NetClient client = restServer.available();
  if (!client)
    return;
  if (client.connected()) {
    if (client.available()) {
      char buff[150];
      buff[client.readBytesUntil('\n', buff, 100)] = 0;
      while (client.read() != -1);
      byte request = RS_INDEX;
      if (strstr_P(buff, (const char*) F("events.json"))) {
        request = RS_EVENTS;
      } else if (strstr_P(buff, (const char*) F("alarm.json"))) {
        request = RS_ALARM;
      } else if (strstr_P(buff, (const char*) F("pumpAlarmReset"))) {
        request = RS_ALARM_RESET;
      } else if (strstr_P(buff, (const char*) F("manualRun"))) {
        request = RS_MANUAL_RUN;
      } else if (strstr_P(buff, (const char*) F("valvesBack"))) {
        request = RS_VALVES_BACK;
      } else if (strstr_P(buff, (const char*) F("saveEvents"))) {
        request = RS_SAVE_EVENTS;
      }
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
        case RS_INDEX:
          printValuesJson(chunked);
          break;
        case RS_EVENTS:
          eventsPrintJson(chunked);
          break;
        case RS_ALARM:
          printAlarmJson(chunked);
          break;
        case RS_ALARM_RESET:
          buttonPressed = true;
          break;
        case RS_MANUAL_RUN:
          manualRunRequest = true;
          break;
        case RS_VALVES_BACK:
          valvesBackStart(0);
          break;
        case RS_SAVE_EVENTS:
          eventsSave();
          break;
      }
      chunked.end();
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

