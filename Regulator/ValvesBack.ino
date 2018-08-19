
const unsigned int VALVE_ROTATION_TIME = 30000; // 30 sec
#ifndef ___AVR___
const int TEMP_SENS_WARM = 1000;
#else
const int TEMP_SENS_WARM = 580;
#endif

unsigned long valvesBackTime = 0;

void valvesBackSetup() {
  pinMode(VALVES_RELAY_PIN, OUTPUT);
  digitalWrite(VALVES_RELAY_PIN, LOW);
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
    unsigned short v = valvesBackTempSensRead();
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

// esp8266 WiFi disconnects if analogRead is used hard
unsigned short valvesBackTempSensRead() {

  const unsigned long MEASURE_INTERVAL = 10L * 1000 * 60; // 10 minutes
  static unsigned long lastMeasureMillis;
  static unsigned short lastValue;

  if (loopStartMillis - lastMeasureMillis > MEASURE_INTERVAL) {
    lastMeasureMillis = loopStartMillis;
    lastValue = analogRead(TEMPSENS_PIN);
  }
  return lastValue;
}

