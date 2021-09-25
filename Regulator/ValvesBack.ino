
unsigned long valvesBackTime = 0;

void valvesBackSetup() {
  pinMode(VALVES_RELAY_PIN, OUTPUT);
  digitalWrite(VALVES_RELAY_PIN, LOW);
}

void valvesBackReset() {
  if (valvesBackRelayOn) {
    digitalWrite(VALVES_RELAY_PIN, LOW);
    valvesBackRelayOn = false;
  }
  valvesBackTime = 0;
}

void valvesBackLoop() {

  const int BOILER_TEMP_WARM = 25;
  const int BOILER_TEMP_HOT = 65;
  const byte VALVES_BACK_HOUR = 5;

  if (!mainRelayOn && !valvesBackTime) {
    unsigned short v = valvesBackBoilerTemperature();
    if ((v > BOILER_TEMP_WARM && v < BOILER_TEMP_HOT) || hourNow == VALVES_BACK_HOUR) {
      valvesBackStart(v);
    }
  }
  if (valvesBackRelayOn && (loopStartMillis - valvesBackTime) > VALVE_ROTATION_TIME) {
    msg.print(" VR_off");
    digitalWrite(VALVES_RELAY_PIN, LOW);
    valvesBackRelayOn = false;
  }
}

void valvesBackStart(int v) {
  if (mainRelayOn)
    return;
  msg.print(" VR_on");
  digitalWrite(VALVES_RELAY_PIN, HIGH);
  valvesBackRelayOn = true;
  valvesBackTime = loopStartMillis;
  eventsWrite(VALVES_BACK_EVENT, v, 0);
}

boolean valvesBackExecuted() {
  return valvesBackTime > 0;
}

unsigned short valvesBackBoilerTemperature() {

  const unsigned long MEASURE_INTERVAL = 10L * 1000 * 60; // 10 minutes
  static unsigned long lastMeasureMillis;
  static unsigned short lastValue;

  if (loopStartMillis - lastMeasureMillis > MEASURE_INTERVAL) {
    lastMeasureMillis = loopStartMillis;
    lastValue = valvesBackRequestBoilerTemperature();
  }
  return lastValue;
}

int valvesBackRequestBoilerTemperature() {
  const IPAddress emsEspAddress(192,168,1,106);
  const int emsEspPort = 23;

  NetClient client;
  if (!client.connect(emsEspAddress, emsEspPort))
    return -1;

  byte b;
  while (client.readBytes(&b, 1));

  client.println("info");
  client.flush();
  client.setTimeout(2000);
  int t = -2;
  if (client.find("Selected flow temperature: ")) {
    t = client.parseInt();
  }
  client.stop();
  return t;
}
