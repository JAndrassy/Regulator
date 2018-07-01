
#define I2C_ADC121         0x50

#ifdef I2C_ADC121
#include <Wire.h>
#define REG_ADDR_RESULT         0x00
#define REG_ADDR_CONFIG         0x02
  const int ELSENS_MIN_ON_VALUE = 40;
#else
  const int ELSENS_MIN_ON_VALUE = 10;
#endif
const unsigned long OVERHEATED_COOLDOWN_TIME = PUMP_STOP_MILLIS - 30000; // resume 30 sec before pump stops

unsigned long overheatedStart = 0;

void elsensSetup() {
#ifdef I2C_ADC121
  Wire.begin();
  Wire.beginTransmission(I2C_ADC121);
  Wire.write(REG_ADDR_CONFIG);
  Wire.write(REG_ADDR_RESULT);
  Wire.endTransmission();
  Wire.beginTransmission(I2C_ADC121);
  Wire.write(REG_ADDR_RESULT);
  Wire.endTransmission();
#endif
}

void elsensLoop() {

#ifdef I2C_ADC121
  const int ELSENS_MAX_VALUE = 1900;
  const float ELSENS_VALUE_COEF = 1.1;
  const int ELSENS_MIN_HEATING_VALUE = 300;
#elif defined(ESP8266)
  const int ELSENS_MAX_VALUE = 500;
  const float ELSENS_VALUE_COEF = 4.15;
  const int ELSENS_MIN_HEATING_VALUE = 80;
#else
  const int ELSENS_MAX_VALUE = 230;
  const float ELSENS_VALUE_COEF = 11.13;
  const int ELSENS_MIN_HEATING_VALUE = 60;
#endif
  const float PF_ANGLE_SHIFT = -0.04 * PI;
  const float PF_ANGLE_INTERVAL = 0.33 * PI;

  elsens = readElSens();

  // waiting for water to cooldown
  if (overheatedStart != 0) {
    if ((loopStartMillis - overheatedStart) < OVERHEATED_COOLDOWN_TIME && !buttonPressed)
      return;
    overheatedStart = 0;
    state = RegulatorState::MONITORING;
  }

  if (heatingPower > 0 && elsens < ELSENS_MIN_HEATING_VALUE) {
    if (heatingPower < 400) { // low power fall out
      heatingPower = 0; // heating can start again with min_start_power
      msg.print(F("fall out"));
    } else {
      overheatedStart = loopStartMillis;
      state = RegulatorState::OVERHEATED;
      msg.print(F("overheated"));
    }
    eventsWrite(OVERHEATED_EVENT, elsens, heatingPower);
    alarmSound();
  }
  if (!mainRelayOn) {
    elsensPower = 0;
  } else {
    float ratio = 1.0 - ((float) elsens / ELSENS_MAX_VALUE); // to 'guess' the 'power factor'
    elsensPower = (int) (elsens * ELSENS_VALUE_COEF * cos(PF_ANGLE_SHIFT + (ratio * PF_ANGLE_INTERVAL)));
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
    int v = elsensAnalogRead();
    if (v > 4 && countOf0 > 10)
      break;
    if (v <= 4) {
      countOf0++;
    }
  }
  if (countOf0 < 10) // sensor is not connected, pin is floating
    return -1;

  // sample AC
  long sum = 0;
  int n = 0;
  start_time = millis();
  while(millis() - start_time < 400) { // in 400 ms measures 20 50Hz AC oscillations
    sum += elsensAnalogRead();
    n++;
  }
  return sum / (n / 2); // half of the values are zeros for removed negative voltages
}

unsigned short elsensAnalogRead() {
#ifdef I2C_ADC121
  Wire.requestFrom(I2C_ADC121, 2);
  byte buff[2];
  Wire.readBytes(buff, 2);
  return (buff[0] << 8) | buff[1];
#else
  return analogRead(ELSENS_PIN);
#endif
}
