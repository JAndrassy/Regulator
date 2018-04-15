
const unsigned long MANUAL_RUN_MILLIS = 90 * 60000; // 1.5 h

unsigned long manualRunStart = 0;

boolean manualRunLoop() {
  if (manualRunRequest) { // from UI
    manualRunRequest = false;
    if (manualRunStart != 0) {
      manualRunStart = 0; // stop
    } else if (turnMainRelayOn()) {
      manualRunStart = loopStartMillis;
      digitalWrite(BYPASS_RELAY_PIN, HIGH);
      bypassRelayOn = true;
      heatingPower = BYPASS_POWER;
      state = RegulatorState::MANUAL_RUN;
      eventsWrite(MANUAL_RUN_EVENT, 0, 0);
    }
  }
  return (manualRunStart != 0 && (loopStartMillis - manualRunStart) < MANUAL_RUN_MILLIS);
}

byte manualRunMinutesLeft() {
  return (MANUAL_RUN_MILLIS - (loopStartMillis - manualRunStart)) / 60000;
}
