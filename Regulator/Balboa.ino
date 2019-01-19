
const int INVERTER_MAX_AC_POWER = 5000; // W
const int BALBOA_HEATER_POWER = 3000; // W

void balboaSetup() {
  pinMode(BALBOA_RELAY_PIN, OUTPUT);
}

void balboaReset() {
  digitalWrite(BALBOA_RELAY_PIN, LOW);
  balboaRelayOn = false;
}

void balboaLoop() {
  const byte WAIT_FOR_IT_COUNT = 20; // more then a minute and half
  static byte waitForItCounter = 0; // to not react on short spikes

  int hhc = inverterAC - meterPower - heatingPower; // household consumption without heater
  if (balboaRelayOn) { // if relay on, balboa heater is paused
    hhc += BALBOA_HEATER_POWER; // added for evaluation
  } else {
    if (inverterAC < (hhc - BALBOA_HEATER_POWER)) // don't care, if PV can't cover hhc
      return;
  }


  boolean pause = hhc >= INVERTER_MAX_AC_POWER;
  if (balboaRelayOn != pause) {
    if (pause && (waitForItCounter < WAIT_FOR_IT_COUNT)) {
      waitForItCounter++;
    } else {
      digitalWrite(BALBOA_RELAY_PIN, pause);
      balboaRelayOn = pause;
      if (pause) {
        eventsWrite(BALBOA_PAUSE_EVENT, inverterAC, meterPower);
        waitForItCounter = 0;
        alarmSound();
      }
    }
  } else {
    waitForItCounter = 0;
  }
}
