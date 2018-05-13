
const int ELSENS_MIN_ON_VALUE = 110;
const int ELSENS_MIN_HEATING_VALUE = 800;
const unsigned long OVERHEATED_COOLDOWN_TIME = PUMP_STOP_MILLIS - 30000; // resume 30 sec before pump stops

unsigned long overheatedStart = 0;

void elsensLoop() {

  elsens = readElSens();

  // waiting for water to cooldown
  if (overheatedStart != 0) {
    if ((loopStartMillis - overheatedStart) < OVERHEATED_COOLDOWN_TIME && !buttonPressed)
      return;
    overheatedStart = 0;
    state = RegulatorState::MONITORING;
  }

  if (heatingPower > 0 && elsens < ELSENS_MIN_HEATING_VALUE) {
    overheatedStart = loopStartMillis;
    state = RegulatorState::OVERHEATED;
    msg.print(F("overheated"));
    eventsWrite(OVERHEAT_EVENT, elsens, 0);
    alarmSound();
    return;
  }
  if (heatingPower == 0 || bypassRelayOn) {
    elsensPower = 0;
  } else {
    float ratio = (float) elsens / 2400;
    elsensPower = (int) ((float) elsens * sin(ratio * PI/2));
  }
}

boolean elsensCheckPump() {
  delay(1000); // pump run-up
  int v = readElSens();
  if (v < ELSENS_MIN_ON_VALUE) {
    digitalWrite(MAIN_RELAY_PIN, LOW);
    mainRelayOn = false;
    alarmCause = AlarmCause::PUMP;
    eventsWrite(PUMP_EVENT, v, ELSENS_MIN_ON_VALUE);
    return false;
  }
  return true;
}

byte overheatedSecondsLeft() {
  return (OVERHEATED_COOLDOWN_TIME - (loopStartMillis - overheatedStart)) / 1000;
}

/**
 * Grove Electricity Sensor module removes the negative part of the AC oscillation
 * zero crossing is where the removed part ends (sequence of zero readings)
 */
int readElSens() {

  // wait for zero crossing
  byte countOf0 = 0;
  long start_time = millis();
  while (millis() - start_time < 40) {
    int v = analogRead(ELSENS_PIN);
    if (v > 0 && countOf0 > 10)
      break;
    if (v == 0) {
      countOf0++;
    }
  }
  if (countOf0 < 10) // sensor is not connected, pin is floating
    return ELSENS_MIN_ON_VALUE;

  // sample AC
  long sum = 0;
  int n = 0;
  start_time = millis();
  while(millis() - start_time < 400) { // in 400 ms measures 20 50Hz AC oscillations
    sum += analogRead(ELSENS_PIN);
    n++;
  }
  return sum * 10 / (n / 2); // half of the values are zeros for removed negative voltages
}
