
//#define BLYNK_PRINT Serial

#include <BlynkSimpleWiFiLink.h>
#include <BlynkWidgets.h>

#define GAUGE_WIDGET V0
#define LCD_WIDGET V1
#define TERINAL_WIDGET V2
#define MANUAL_RUN_BUTTON V3
#define VALVES_BACK_BUTTON V4

WidgetLCD lcd(V1);

BLYNK_READ(GAUGE_WIDGET) {
  updateWidgets();
}

BLYNK_WRITE(MANUAL_RUN_BUTTON) {
  manualRunRequest = true;
}

BLYNK_WRITE(VALVES_BACK_BUTTON) {
  if (param.asInt()) {
    valvesBackStart(0);
    updateWidgets();
  }
}

void blynkSetup() {
  Blynk.config(MY_BLYNK_TOKEN, BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT);
 // we do not wait for connection. it is established later in loop
}

void blynkLoop() {
  Blynk.run();
}

void updateWidgets() {
  sprintfF(msg, F("Blynk"));
  Blynk.virtualWrite(GAUGE_WIDGET, heatingPower);
  char buff[17];
  sprintfF(buff, F("%c %d%d%d%d %c M% 5dW"), (char) state, mainRelayOn, bypassRelayOn, balboaRelayOn, valvesRelayOn,
      valvesBackExecuted() ? 'B' : ' ', m);
  lcd.print(0, 0, buff);
  switch (state) {
    case RegulatorState::MONITORING:
    case RegulatorState::REGULATING:
    case RegulatorState::OVERHEATED:
      if (b < -20 || b > 90) {
        sprintfF(buff, F("BAT%3d%%   % 5dW"), soc, b);
      } else {
        sprintfF(buff, F("I% 5dW  S% 5dW"), inverterAC, elsensPower);
      }
      break;
    case RegulatorState::REST:
      sprintfF(buff, F("temp.sensor % 4d"), analogRead(TEMPSENS_PIN));
      break;
    case RegulatorState::MANUAL_RUN:
      sprintfF(buff, F("run time % 3d min"), manualRunMinutesLeft());
      break;
    case RegulatorState::ALARM:
      sprintfF(buff, F("ALARM! cause: %c "), alarmCause);
      break;
  }
  lcd.print(0, 1, buff);
}

