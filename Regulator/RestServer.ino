
NetServer restServer(81);

void restServerSetup() {
  restServer.begin();
}

void restServerLoop() {
  NetClient client = restServer.available();
  if (!client)
    return;
  if (client.connected()) {
    if (client.available()) {
      char buff[100];
      buff[client.readBytesUntil('\n', buff, 100)] = 0;
      while (client.read() != -1);
      client.println(F("HTTP/1.1 200 OK"));
      client.println(F("Content-Type: application/json"));
      client.println(F("Connection: close"));
      client.println(F("Cache-Control: no-store"));
      client.println(F("Access-Control-Allow-Origin: *"));
      client.println();
      if (strstr_P(buff, (const char*) F("events.json"))) {
        eventsPrintJson(client);
      } else if (strstr_P(buff, (const char*) F("alarm.json"))) {
        printAlarmJson(client);
      } else if (strstr_P(buff, (const char*) F("pumpAlarmReset"))) {
        buttonPressed = true;
      } else if (strstr_P(buff, (const char*) F("manualRun"))) {
        manualRunRequest = true;
      } else if (strstr_P(buff, (const char*) F("valvesBack"))) {
        valvesBackStart(0);
      } else if (strstr_P(buff, (const char*) F("saveEvents"))) {
        eventsSave();
      } else {
        printValuesJson(client);
      }
      client.flush();
    }
  }
  client.stop();
}

void printValuesJson(Print& client) {
  char buff[100];
  sprintfF(buff,
      F("{\"st\":\"%c\",\"v\":\"%s\",\"r\":\"%d %d %d %d\",\"ec\":%d,\"ts\":%d"),
      state, version, mainRelayOn, bypassRelayOn, balboaRelayOn,
      valvesRelayOn, eventsRealCount(), analogRead(TEMPSENS_PIN));
  client.print(buff);
  switch (state) {
    case RegulatorState::REGULATING:
    case RegulatorState::OVERHEATED:
      sprintfF(buff, F(",\"h\":%d,\"a\":%d"), heatingPower, availablePower);
      client.print(buff);
      /* no break */
    case RegulatorState::MONITORING:
      sprintfF(buff, F(",\"m\":%d,\"i\":%d,\"soc\":%d,\"b\":%d"), m, inverterAC, soc, b);
      client.print(buff);
      break;
    case RegulatorState::MANUAL_RUN:
      sprintfF(buff, F(",\"mr\":%u"), manualRunMinutesLeft());
      client.print(buff);
      break;
    default:
      break;
  }
  client.print('}');
}

void printAlarmJson(Print& client) {
  client.print(F("{\"a\":\""));
  client.print((char) alarmCause);
  client.print('"');
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

