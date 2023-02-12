
bool manualBalboaPause;

void balboaSetup() {
  pinMode(BALBOA_RELAY_PIN, OUTPUT);
}

void balboaReset() {
  if (manualBalboaPause)
    return;
  digitalWrite(BALBOA_RELAY_PIN, LOW);
  balboaRelayOn = false;
}

void balboaLoop() {
  const int CONSUMPTION_POWER_LIMIT = 5000; // W
  const int BALBOA_HEATER_POWER = 3000; // W
  const unsigned long WAIT_FOR_IT_MILLIS = 86000; // 86 seconds. 4 seconds less then a minute and half
  static unsigned long waitForItTimer = 0; // to not react on short spikes

  if (manualBalboaPause)
    return;

  int hhc = inverterAC - meterPower - heatingPower; // household consumption without heater
  if (balboaRelayOn) { // if relay on, balboa heater is paused
    hhc += BALBOA_HEATER_POWER; // added for evaluation
  } else {
    if (inverterAC < (hhc - BALBOA_HEATER_POWER)) // don't care, if PV can't cover hhc
      return;
  }

  boolean pause = hhc >= CONSUMPTION_POWER_LIMIT;
  if (balboaRelayOn != pause) {
    if (!pause) {
      digitalWrite(BALBOA_RELAY_PIN, LOW);
      balboaRelayOn = false;
    } else if (!waitForItTimer) {
      waitForItTimer = loopStartMillis; // start the timer
    } else if (loopStartMillis - waitForItTimer > WAIT_FOR_IT_MILLIS) {
      waitForItTimer = 0;
      digitalWrite(BALBOA_RELAY_PIN, HIGH);
      balboaRelayOn = true;
      eventsWrite(BALBOA_PAUSE_EVENT, inverterAC, meterPower);
      alarmSound();
    }
  } else {
    waitForItTimer = 0;
  }
}

void balboaManualPause(bool pause) {
  manualBalboaPause = pause;
  digitalWrite(BALBOA_RELAY_PIN, manualBalboaPause ? HIGH : LOW);
  balboaRelayOn = manualBalboaPause;
}
