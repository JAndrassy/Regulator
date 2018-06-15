
const int ELSENS_MIN_ON_VALUE = 110;
const unsigned long OVERHEATED_COOLDOWN_TIME = PUMP_STOP_MILLIS - 30000; // resume 30 sec before pump stops

unsigned long overheatedStart = 0;

void elsensLoop() {

#ifdef ESP8266
  const int ELSENS_MAX_VALUE = 5000;
  const float ELSENS_VALUE_COEF = 0.0016;
  const int ELSENS_MIN_HEATING_VALUE = 800;
#else
  const int ELSENS_MAX_VALUE = 2300;
  const float ELSENS_VALUE_COEF = 0.0043;
  const int ELSENS_MIN_HEATING_VALUE = 600;
#endif

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
    eventsWrite(OVERHEATED_EVENT, elsens, 0);
    alarmSound();
    return;
  }
  if (!mainRelayOn) {
    elsensPower = 0;
  } else {
    float elsens0 = elsens + (ELSENS_MAX_VALUE * 0.1); // I don't know why, but only with this the result is exact
    float ratio = 1 - (elsens0 / ELSENS_MAX_VALUE); // to 'guess' the 'power factor'
    elsensPower = (int) (elsens0 * voltage * ELSENS_VALUE_COEF * cos(ratio * PI/2));
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
    if (v > 4 && countOf0 > 10)
      break;
    if (v <= 4) {
      countOf0++;
    }
  }
  if (countOf0 < 10) // sensor is not connected, pin is floating
    return -1; //ELSENS_MIN_ON_VALUE;

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
