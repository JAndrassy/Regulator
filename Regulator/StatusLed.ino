
void statusLedSetup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
}

void statusLedLopp() {
  const int BLINK_INTERVAL = 1000;
  static unsigned long previousMillis = 0;
  static boolean blinkLedState = false;

#ifdef ___AVR___
#define PWMRANGE 256
#else
#define PWMRANGE 1024
#endif

  if (loopStartMillis - previousMillis < BLINK_INTERVAL)
    return;
  previousMillis = loopStartMillis;
  blinkLedState = !blinkLedState;
  if (blinkLedState) {
    if (state == RegulatorState::REST || state == RegulatorState::OVERHEATED) {
      analogWrite(STATUS_LED_PIN, PWMRANGE / 2);
    } else {
      digitalWrite(STATUS_LED_PIN, HIGH);
    }
    switch (alarmCause) {
      case AlarmCause::MODBUS:
        statusLedShortBlink();
        /* no break */
      case AlarmCause::NETWORK:
        statusLedShortBlink();
        /* no break */
      case AlarmCause::PUMP:
        statusLedShortBlink();
        /* no break */
      default:
        break;
    }
  } else {
    if (state == RegulatorState::REGULATING || state == RegulatorState::MANUAL_RUN) {
      digitalWrite(STATUS_LED_PIN, LOW);
    } else {
      analogWrite(STATUS_LED_PIN, PWMRANGE / 4);
    }
  }
}

void statusLedShortBlink() {
  delay(100);
  digitalWrite(STATUS_LED_PIN, LOW);
  delay(100);
  digitalWrite(STATUS_LED_PIN, HIGH);
}
