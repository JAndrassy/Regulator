#include <Grove_LED_Bar.h>

Grove_LED_Bar ledBar(LEDBAR_CLOCK_PIN, LEDBAR_DATA_PIN, true);

void ledBarSetup() {
  ledBar.begin();

  // indicate startup
  ledBar.setLed(5, 1);
  ledBar.setLed(6, 1);
}

void ledBarLoop() {

  const byte ALARM_LED = 10; //red
  const byte BLINK_LED = 9;  // yellow
  const byte BYPASS_LED = 8; //green
  const byte MAX_LEVEL_LED = 7; // green

  const int REFRESH_INTERVAL = 1000;
  static unsigned long previousMillis = 0;
  static boolean blinkLedState = false;

  if (loopStartMillis - previousMillis < REFRESH_INTERVAL)
    return;
  previousMillis = loopStartMillis;
  blinkLedState = !blinkLedState;
  float level = 0;
  switch (state) {
    case RegulatorState::ALARM:
      switch (alarmCause) {
        case AlarmCause::PUMP:
          level = 1;
          break;
        case AlarmCause::NETWORK:
          level = 2;
          break;
        case AlarmCause::MODBUS:
          level = 3;
          break;
        default:
          break;
      }
      break;
    case RegulatorState::OVERHEATED:
      level = (float) overheatedSecondsLeft() / 60; // number of minutes
      break;
    case RegulatorState::REGULATING:
      level = MAX_LEVEL_LED - MAX_LEVEL_LED * ((float) (MAX_POWER - heatingPower) / MAX_POWER);
      break;
    case RegulatorState::MANUAL_RUN:
      level = (float) manualRunMinutesLeft() / 15; // number of quarter-hours
      break;
    default:
      break;
  }
  ledBar.setLevel(level);
  if (bypassRelayOn) {
    ledBar.setLed(BYPASS_LED, state == RegulatorState::MANUAL_RUN && blinkLedState ? 0.5 : 1);
  }
  if (valvesBackExecuted()) {
    ledBar.setLed(5, valvesBackRelayOn ? 1 : 0.25);
  }
  if (state == RegulatorState::ALARM) {
    ledBar.setLed(ALARM_LED, 1);
  }
  if (blinkLedState) {
    ledBar.setLed(BLINK_LED, 0.75);
    if (state == RegulatorState::OVERHEATED) {
      ledBar.setLed(ALARM_LED, 0.75);
    }
  } else {
    ledBar.setLed(BLINK_LED, 0.25);
  }
}
