const int MIN_POWER = 300;

void pilotLoop() {

  const byte MONITORING_UNTIL_SOC = 85; // %
  const byte TOP_OSCILLATION_SOC = 97; // %
  const int MIN_START_POWER = 700;
  const int BYPASS_MIN_START_POWER = BYPASS_POWER + 100;
  const byte WAIT_FOR_IT_COUNT = 3;

  static byte waitForItCounter = 0; // to not react on short spikes

  switch (state) {
    case RegulatorState::MONITORING:
      break;
    case RegulatorState::REGULATING:
      if (!mainRelayOn) {
        state = RegulatorState::MONITORING;
      }
      break;
    default:
      return;
  }

  // check state of charge
  if (pvSOC < MONITORING_UNTIL_SOC) // %
    return;

  // sum available power
  int pvChP = (pvChargingPower > 0 || pvSOC > TOP_OSCILLATION_SOC) ? 0 : pvChargingPower;
  availablePower = heatingPower + meterPower + pvChP - (mainRelayOn ? 0 : PUMP_POWER);
  if (heatingPower == 0 && availablePower < MIN_START_POWER) {
    waitForItCounter = 0;
    return;
  }
  if (bypassRelayOn && availablePower > BYPASS_POWER)
    return;
  if (availablePower < MIN_POWER) {
    heatingPower = 0;
    waitForItCounter = 0;
    return;
  }
  if (availablePower - heatingPower > 0 && waitForItCounter < WAIT_FOR_IT_COUNT) {
    waitForItCounter++;
    return;
  }
  waitForItCounter = 0;

  if (!turnMainRelayOn())
    return;
  state = RegulatorState::REGULATING;

  // bypass the power regulator module for max power
  boolean bypass = availablePower > (bypassRelayOn ? BYPASS_POWER : BYPASS_MIN_START_POWER);
  if (bypass != bypassRelayOn) {
    digitalWrite(BYPASS_RELAY_PIN, bypass);
    bypassRelayOn = bypass;
  }

  // set heating power
  pwm = bypass ? 0 : power2pwm(availablePower);
  analogWrite(PWM_PIN, pwm);
  heatingPower = bypass ? BYPASS_POWER : (availablePower > MAX_POWER) ? MAX_POWER : availablePower;
}

unsigned short power2pwm(int power) {

  const float MAX_CURRENT = 8.8;
  const float POWER2PWM_KOEF = 30.0;
  const float PF_ANGLE_SHIFT = 0.08 * PI;
  const float PF_ANGLE_INTERVAL = 0.33 * PI;
  const unsigned short MIN_PWM = 190;

  if (power < MIN_POWER)
    return 0;
  unsigned short res;
  if (power >= MAX_POWER) {
    res = 1023;
  } else {
    float current = (float) power / voltage;
    float ratio = current / MAX_CURRENT;
    res = MIN_PWM + POWER2PWM_KOEF * current / cos(PF_ANGLE_SHIFT + (ratio * PF_ANGLE_INTERVAL));
  }
#ifdef ___AVR___
  res /= 4;
#endif
  return res;
}
