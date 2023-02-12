
// https://github.com/jandrassy/Regulator/wiki/Blynk

#define GAUGE_WIDGET V0
#define STATE_WIDGET V1
#define TABLE_WIDGET V2
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
#define STATS_TABLE_WIDGET V22

BLYNK_READ(GAUGE_WIDGET) {
  updateWidgets();
}

BLYNK_WRITE(MANUAL_RUN_BUTTON) {
  manualRunRequest = !manualRunRequest;
  updateWidgets();
}

BLYNK_WRITE(VALVES_BACK_BUTTON) {
  if (param.asInt()) {
    valvesBackStart(0);
    updateWidgets();
  }
}

BLYNK_WRITE(BALBOA_PAUSE_BUTTON) {
  balboaManualPause();
  updateWidgets();
}

BLYNK_WRITE(POWERPILOT_PLAN_SELECTOR) {
  powerPilotSetPlan(param.asInt() - 1); // Blynk Select index starts at 1
  updateWidgets();
}

void blynkSetup() {
#if defined(ETHERNET) && !defined(UIP_CONNECT_TIMEOUT)
  _blynkEthernetClient.setConnectionTimeout(4000);
#endif
  Blynk.config(SECRET_BLYNK_TOKEN, BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT);
 // we do not wait for connection. it is established later in loop
}

void blynkLoop() {
  Blynk.run();
}

void updateWidgets() {
//  msg.print(F("Blynk"));
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
  Blynk.virtualWrite(POWERPILOT_PLAN_SELECTOR, powerPilotPlan + 1);
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
  eventsBlynk();
  statsBlynk();
}

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
