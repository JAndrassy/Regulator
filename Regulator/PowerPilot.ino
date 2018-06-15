
const byte MAP_POINTS_COUNT = 6;
unsigned short power2pwmPoints[MAP_POINTS_COUNT][2] = {
  { 380, 213},
  { 585, 243},
  {1190, 383},
  {1741, 683},
  {1921, 863},
  {MAX_POWER, 1023}
};
const int MIN_POWER = power2pwmPoints[0][0];

void pilotLoop() {

  const byte MONITORING_UNTIL_SOC = 85; // %
  const int MIN_START_POWER = MIN_POWER + 300;
  const int BYPASS_MIN_START_POWER = BYPASS_POWER + 100;
  const byte WAIT_FOR_IT_COUNT = 2;

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
  if (soc < MONITORING_UNTIL_SOC) // %
    return;

  // sum available power
  availablePower = heatingPower + m + ((b > 0 || soc > 97) ? 0 : b) - (mainRelayOn ? 0 : PUMP_POWER);
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
    analogWrite(PWM_PIN, 0); // unpower for relay flip
    delay(100);
    digitalWrite(BYPASS_RELAY_PIN, bypass);
    bypassRelayOn = bypass;
  }

  // set heating power
  pwm = bypass ? 0 : power2pwm(availablePower);
  analogWrite(PWM_PIN, pwm);
  heatingPower = bypass ? BYPASS_POWER : (availablePower > MAX_POWER) ? MAX_POWER : availablePower;
}

unsigned short power2pwm(int power) {
  if (power < MIN_POWER)
    return 0;
  if (power >= MAX_POWER)
    return power2pwmPoints[MAP_POINTS_COUNT - 1][1];
  int i = 0;
  for (; i < MAP_POINTS_COUNT - 1; i++) {
    if (power >= power2pwmPoints[i][0] && power < power2pwmPoints[i + 1][0])
      break;
  }
  unsigned short res = map(power, power2pwmPoints[i][0], power2pwmPoints[i + 1][0], power2pwmPoints[i][1], power2pwmPoints[i+1][1]);
#ifndef ESP8266
  res /= 4;
#endif
  return res;
}

