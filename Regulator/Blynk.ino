
#define GAUGE_WIDGET V0
#define STATE_WIDGET V1
#define REFRESH_BUTTON V2
#define MANUAL_RUN_BUTTON V3
#define VALVES_BACK_BUTTON V4
#define CONSUMED_WIDGET V5
#define BATT_SOC_WIDGET V6
#define METER_WIDGET V7
#define CHARGING_WIDGET V8
#define CALIBRATING_WIDGET V9
#define MAIN_RELAY_WIDGET V10
#define BYPASS_RELAY_WIDGET V11
#define BALBOA_RELAY_WIDGET V12
#define VALVES_BACK_WIDGET V13
#define HH_CONSUMPTION_WIDGET V14
#define PV_PRODUCTION_WIDGET V15
#define MANUAL_RUN_WIDGET V16
#define BOILER_TEMP_WIDGET V17
#define BALBOA_PAUSE_BUTTON V18
#define POWERPILOT_PLAN_SELECTOR V19
//#define STATS_TABLE_WIDGET V20

// REFRESH_BUTTON is a workaround for not working BLYNK_APP_CONNECTED/BLYNK_APP_DISCONNECTED

unsigned long blynkRefreshStart = 0;

BLYNK_CONNECTED() {
  Blynk.syncVirtual(REFRESH_BUTTON);
}

BLYNK_WRITE(REFRESH_BUTTON) {
  blynkRefreshStart = param.asInt() ? millis() : 0;
  if (!blynkRefreshStart) {
    clearWidgets();
  }
}

BLYNK_WRITE(MANUAL_RUN_BUTTON) {
  manualRunRequest = param.asInt();
  manualRunLoop();
  updateWidgets();
}

BLYNK_WRITE(VALVES_BACK_BUTTON) {
  if (param.asInt()) {
    valvesBackStart(0);
    updateWidgets();
    Blynk.virtualWrite(VALVES_BACK_BUTTON, 0);
  }
}

BLYNK_WRITE(BALBOA_PAUSE_BUTTON) {
  balboaManualPause(param.asInt());
  updateWidgets();
}

BLYNK_WRITE(POWERPILOT_PLAN_SELECTOR) {
  powerPilotSetPlan(param.asInt());
  updateWidgets();
}

void blynkSetup() {
#if defined(ETHERNET) && !defined(UIP_CONNECT_TIMEOUT)
  _blynkEthernetClient.setConnectionTimeout(4000);
#endif
  Blynk.config(SECRET_BLYNK_TOKEN);
 // we do not wait for connection. it is established later in loop
}

void blynkLoop() {
  const unsigned long REFRESH_MILLIS = 10000; // 10 seconds
  const unsigned long REFRESH_INTERVAL = 3UL * 60000; // 3 minutes
  static unsigned long previousMillis = 0;

  if (blynkRefreshStart) {
    if (millis() - previousMillis > REFRESH_MILLIS) {
      previousMillis = millis();
      updateWidgets();
    }
    if (loopStartMillis - blynkRefreshStart > REFRESH_INTERVAL) {
      blynkRefreshStart = 0;
      clearWidgets();
    }
  }
  Blynk.run();
}

void updateWidgets() {
//  msg.print(F("Blynk"));
  Blynk.beginGroup();
  Blynk.virtualWrite(REFRESH_BUTTON, blynkRefreshStart ? 0xFF : 0);
  Blynk.virtualWrite(PV_PRODUCTION_WIDGET, inverterAC + pvChargingPower);
  Blynk.virtualWrite(HH_CONSUMPTION_WIDGET, inverterAC - meterPower - elsensPower);
  Blynk.virtualWrite(METER_WIDGET, meterPower);
  Blynk.virtualWrite(CHARGING_WIDGET, pvChargingPower);
  Blynk.virtualWrite(CALIBRATING_WIDGET, pvBattCalib ? 0xFF : 0);
  Blynk.virtualWrite(BATT_SOC_WIDGET, pvSOC);
  Blynk.virtualWrite(GAUGE_WIDGET, heatingPower);
  Blynk.virtualWrite(CONSUMED_WIDGET, statsConsumedPowerToday());
  Blynk.virtualWrite(MAIN_RELAY_WIDGET, mainRelayOn ? 0xFF : 0);
  Blynk.virtualWrite(BYPASS_RELAY_WIDGET, bypassRelayOn  ? 0xFF : 0);
  Blynk.virtualWrite(BALBOA_RELAY_WIDGET, balboaRelayOn ? 0xFF : 0);
  Blynk.virtualWrite(VALVES_BACK_WIDGET, valvesBackExecuted() ? 0xFF : 0);
  Blynk.virtualWrite(BOILER_TEMP_WIDGET, valvesBackBoilerTemperature());
  Blynk.virtualWrite(MANUAL_RUN_WIDGET, (short) manualRunMinutesLeft());
  Blynk.virtualWrite(MANUAL_RUN_BUTTON, state == RegulatorState::MANUAL_RUN || manualRunRequest);
  Blynk.virtualWrite(BALBOA_PAUSE_BUTTON, balboaRelayOn);
  Blynk.virtualWrite(POWERPILOT_PLAN_SELECTOR, powerPilotPlan);
  char buff[17];
  CStringBuilder sb(buff, sizeof(buff));
  switch (state) {
    case RegulatorState::MONITORING:
      sb.print(F("Monitoring"));
      break;
    case RegulatorState::REGULATING:
      sb.print(F("Regulating"));
      break;
    case RegulatorState::OVERHEATED:
      sb.printf(F("Overheated %d s"), overheatedSecondsLeft());
      break;
    case RegulatorState::REST:
      sb.print(F("Rest"));
      break;
    case RegulatorState::MANUAL_RUN:
      sb.printf(F("Manual left %d m"), manualRunMinutesLeft());
      break;
    case RegulatorState::ALARM:
      sb.printf(F("ALARM! cause: %c"), alarmCause);
      break;
  }
  Blynk.virtualWrite(STATE_WIDGET, buff);
//  statsBlynk(); no Table widget in Blynk IoT yet
  Blynk.endGroup();
}

void clearWidgets() {
  Blynk.beginGroup();
  Blynk.virtualWrite(REFRESH_BUTTON, 0);
  Blynk.virtualWrite(PV_PRODUCTION_WIDGET, 0);
  Blynk.virtualWrite(HH_CONSUMPTION_WIDGET, 0);
  Blynk.virtualWrite(METER_WIDGET, 0);
  Blynk.virtualWrite(CHARGING_WIDGET, 0);
  Blynk.virtualWrite(CALIBRATING_WIDGET, 0);
  Blynk.virtualWrite(BATT_SOC_WIDGET, 0);
  Blynk.virtualWrite(GAUGE_WIDGET, 0);
  Blynk.virtualWrite(CONSUMED_WIDGET, 0);
  Blynk.virtualWrite(MAIN_RELAY_WIDGET, 0);
  Blynk.virtualWrite(BYPASS_RELAY_WIDGET, 0);
  Blynk.virtualWrite(BALBOA_RELAY_WIDGET, 0);
  Blynk.virtualWrite(VALVES_BACK_WIDGET, 0);
  Blynk.virtualWrite(BOILER_TEMP_WIDGET, 0);
  Blynk.virtualWrite(MANUAL_RUN_WIDGET, 0);
  Blynk.virtualWrite(MANUAL_RUN_BUTTON, 0);
  Blynk.virtualWrite(BALBOA_PAUSE_BUTTON, 0);
  Blynk.virtualWrite(POWERPILOT_PLAN_SELECTOR, 0);
  Blynk.virtualWrite(STATE_WIDGET, "-");
  Blynk.endGroup();
}
/*
void blynkChartData() {

  const unsigned long PUSH_INTERVAL = 120000; // 2 minutes

  static unsigned long previousMillis = 0;
  static int n;
  static long productionSum;
  static long usedProductionSum;
  static long inverterConsumedSum;
  static long hhConsumptionSum;
  static long heaterConsumptionSum;
  static long powerPhaseASum;
  static long powerPhaseBSum;
  static long powerPhaseCSum;

  if (!previousMillis) {
    previousMillis = loopStartMillis;
  }

  n++;
  short production = inverterAC + pvChargingPower;
  short toGrid = ((meterPower > 0) ? meterPower : 0);
  productionSum += production;
  usedProductionSum += production - toGrid;
  inverterConsumedSum += inverterAC - toGrid;
  hhConsumptionSum += inverterAC - meterPower;
  heaterConsumptionSum += measuredPower;

  short third = inverterAC / 3;
  powerPhaseASum += meterPowerPhaseA + third;
  powerPhaseBSum += meterPowerPhaseB + third;
  powerPhaseCSum += (meterPowerPhaseC + third) - elsensPower; // heater is on phase C

  if (loopStartMillis - previousMillis >= PUSH_INTERVAL) {
    if (loopStartMillis - previousMillis >= (PUSH_INTERVAL * 5)) { // after rest or alarm state send zeros
      productionSum = 0;
      usedProductionSum = 0;
      inverterConsumedSum = 0;
      hhConsumptionSum = 0;
      heaterConsumptionSum = 0;
      pvSOC = 0;
      powerPhaseASum = 0;
      powerPhaseBSum = 0;
      powerPhaseCSum = 0;
      n = 1;
      previousMillis = loopStartMillis;
    } else {
      previousMillis += PUSH_INTERVAL;
    }

    Blynk.beginGroup();

    Blynk.virtualWrite(V32, productionSum / n); // green line. energy produced by PV panels
    Blynk.virtualWrite(V33, usedProductionSum / n); // yellow fill. the green area above is "to grid"
    Blynk.virtualWrite(V34, inverterConsumedSum / n); // blue fill
    // overlap of blue and yellow is "direct consumption"
    // clear yellow fill is "to battery"
    // clear blue fill is "from battery"

    // black line. household consumption without regulated consumption of heater.
    // black line above blue area is "from grid"
    // blue area above black line is regulated conditional consumption
    long heaterConsumptionAvg = heaterConsumptionSum / n;
    long hhConsumptionAvg = (hhConsumptionSum / n);
    if (state != RegulatorState::MANUAL_RUN) {
      hhConsumptionAvg -= heaterConsumptionAvg;
    }
    Blynk.virtualWrite(V35, hhConsumptionAvg);

    Blynk.virtualWrite(V36, pvSOC);
    Blynk.virtualWrite(V37, pvBattCalib);
    Blynk.virtualWrite(V38, balboaRelayOn);
    Blynk.virtualWrite(V39, state == RegulatorState::OVERHEATED);

    Blynk.virtualWrite(V40, powerPhaseASum / n);
    Blynk.virtualWrite(V41, powerPhaseBSum / n);
    Blynk.virtualWrite(V42, powerPhaseCSum / n);
    Blynk.virtualWrite(V43, heaterConsumptionAvg);

    Blynk.endGroup();

    n = 0;
    productionSum = 0;
    usedProductionSum = 0;
    inverterConsumedSum = 0;
    hhConsumptionSum = 0;
    heaterConsumptionSum = 0;
    powerPhaseASum = 0;
    powerPhaseBSum = 0;
    powerPhaseCSum = 0;
  }
}
*/
