
#ifdef ARDUINO_ARCH_SAMD
const int ELSENS_ANALOG_MIDDLE_VALUE = 533; // over voltage divider
#else
const int ELSENS_ANALOG_MIDDLE_VALUE = 512;
#endif

const unsigned long OVERHEATED_COOLDOWN_TIME = PUMP_STOP_MILLIS - 30000; // resume 30 sec before pump stops

unsigned long overheatedStart = 0;

void elsensSetup() {
#ifdef ARDUINO_ARCH_SAMD
  // changing Arduino defaults (wiring.c)
  while (ADC->STATUS.bit.SYNCBUSY == 1); // wait for synchronization
  ADC->SAMPCTRL.reg = 0x00;
#endif
}

void elsensLoop() {

  const int PUMP_POWER = 40;

  // system's power factor characteristics
  const float PF_ANGLE_INTERVAL = PI * 0.34;
  const float PF_ANGLE_SHIFT = PI * 0.21;

#ifdef ARDUINO_ARCH_SAMD
  // ACS712 20A with voltage divider
  const int ELSENS_MAX_VALUE = 2000;
  const float ELSENS_VALUE_COEF = 1.4;
  const int ELSENS_VALUE_SHIFT = 100;
#else
  // 5 V ATmega 'analog' pin and ACS712 sensor 20A version
  const int ELSENS_MAX_VALUE = 2000;
  const float ELSENS_VALUE_COEF = 1.41;
  const int ELSENS_VALUE_SHIFT = 0;
#endif
  const int ELSENS_MIN_HEATING_VALUE = 250;

  // waiting for water to cooldown
  if (overheatedStart != 0) {
    if (state == RegulatorState::OVERHEATED && (loopStartMillis - overheatedStart) < OVERHEATED_COOLDOWN_TIME && !buttonPressed)
      return;
    overheatedStart = 0;
    state = RegulatorState::MONITORING;
  }

  elsens = readElSens();

  if (heatingPower > 0 && elsens < ELSENS_MIN_HEATING_VALUE) {
    overheatedStart = loopStartMillis;
    state = RegulatorState::OVERHEATED;
    msg.print(F("overheated"));
    eventsWrite(OVERHEATED_EVENT, elsens, heatingPower);
    alarmSound();
  }

  if (elsens > ELSENS_MIN_HEATING_VALUE) {
    float ratio = 1.0 - ((float) elsens / ELSENS_MAX_VALUE); // to 'guess' the 'power factor'
    elsensPower = (int) (elsens * ELSENS_VALUE_COEF * cos(PF_ANGLE_SHIFT + ratio * PF_ANGLE_INTERVAL)) + ELSENS_VALUE_SHIFT;
  } else {
    elsensPower = mainRelayOn ? PUMP_POWER : 0;
  }
}

boolean elsensCheckPump() {

  const int ELSENS_MIN_ON_VALUE = 40;

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
 * return value is RMS of sampled values
 */
int readElSens() {

  // set 1 for Grove El. sensor CT
  const int RMS_INT_SCALE = 10;

  unsigned long long sum = 0;
  int n = 0;
  unsigned long start_time = millis();
  while (millis() - start_time < 200) { // in 200 ms measures 10 50Hz AC oscillations
    long v = analogRead(ELSENS_PIN) - ELSENS_ANALOG_MIDDLE_VALUE;
    sum += v * v;
    n++;
  }
  if (ELSENS_ANALOG_MIDDLE_VALUE == 0) {
    n = n / 2; // half of the values are zeros for removed negative voltages
  }
  return sqrt((double) sum / n) * RMS_INT_SCALE;
}
