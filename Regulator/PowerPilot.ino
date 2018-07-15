//const byte MAP_POINTS_COUNT = 7;
//unsigned short power2pwmPoints[MAP_POINTS_COUNT][2] = {
//  { 500, 223},
//  { 630, 243},
//  {1140, 363},
//  {1300, 443},
//  {1740, 683},
//  {1900, 863},
//  {MAX_POWER, 1023}
//};
//const int MIN_POWER = power2pwmPoints[0][0];
const int MIN_POWER = 300;

void pilotLoop() {

  const byte MONITORING_UNTIL_SOC = 85; // %
  const int MIN_START_POWER = 600;
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
  if (pvSOC < MONITORING_UNTIL_SOC) // %
    return;

  // sum available power
  availablePower = heatingPower + meterPower + (pvChargingPower > 0 ? 0 : pvChargingPower) - (mainRelayOn ? 0 : PUMP_POWER);
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

  const float POWER2PWM_KOEF = 0.16;
  const float PF_ANGLE_SHIFT = 0.05 * PI;
  const float PF_ANGLE_INTERVAL = 0.33 * PI;
  const unsigned short MIN_PWM = 110;

  if (power < MIN_POWER)
    return 0;
  unsigned short res;
  if (power >= MAX_POWER) {
    res = 1023;
  } else {
    float ratio = (float) power / MAX_POWER;
    res = MIN_PWM + POWER2PWM_KOEF * power / cos(PF_ANGLE_SHIFT + (ratio * PF_ANGLE_INTERVAL));
  }
#ifndef ESP8266
  res /= 4;
#endif
  return res;
}

//unsigned short power2pwm(int power) {
//  if (power < MIN_POWER)
//    return 0;
//  if (power >= MAX_POWER)
//    return power2pwmPoints[MAP_POINTS_COUNT - 1][1];
//  int i = 0;
//  for (; i < MAP_POINTS_COUNT - 1; i++) {
//    if (power >= power2pwmPoints[i][0] && power < power2pwmPoints[i + 1][0])
//      break;
//  }
//  unsigned short res = map(power, power2pwmPoints[i][0], power2pwmPoints[i + 1][0], power2pwmPoints[i][1], power2pwmPoints[i+1][1]);
//#ifndef ESP8266
//  res /= 4;
//#endif
//  return res;
//}
