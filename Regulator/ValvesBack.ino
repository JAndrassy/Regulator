
const unsigned int VALVE_ROTATION_TIME = 30000; // 30 sec
const int TEMP_SENS_WARM = 580;

unsigned long valvesBackTime = 0;

void valvesBackSetup() {
  pinMode(VALVES_RELAY_PIN, OUTPUT);
  pinMode(TEMPSENS_PIN, INPUT);
}

void valvesBackReset() {
  if (valvesRelayOn) {
    digitalWrite(VALVES_RELAY_PIN, LOW);
    valvesRelayOn = false;
  }
  valvesBackTime = 0;
}

void valvesBackLoop() {
  if (!mainRelayOn && !valvesBackTime) {
    int v = analogRead(TEMPSENS_PIN);
    if (v > TEMP_SENS_WARM) {
      valvesBackStart(v);
    }
  }
  if (valvesRelayOn && (loopStartMillis - valvesBackTime) > VALVE_ROTATION_TIME) {
    digitalWrite(VALVES_RELAY_PIN, LOW);
    valvesRelayOn = false;
  }
}

void valvesBackStart(int v) {
  if (mainRelayOn)
    return;
  digitalWrite(VALVES_RELAY_PIN, HIGH);
  valvesRelayOn = true;
  valvesBackTime = loopStartMillis;
  eventsWrite(VALVES_BACK_EVENT, v, (v == 0) ? 0 : TEMP_SENS_WARM);
}

boolean valvesBackExecuted() {
  return valvesBackTime > 0;
}

