
const int EXT_HEATER_POWER_MIN = EXT_HEATER_POWER - 100;

enum {
  WEMO_OP_OFF,
  WEMO_OP_ON,
  WEMO_OP_QUERY
};

void extHeaterLoop() {

  const int EXT_HEATER_START_POWER = MAX_POWER + EXT_HEATER_POWER - 200;
  const int EXT_HEATER_STOP_POWER = EXT_HEATER_START_POWER - 500;
  const unsigned long HEATER_CHECK_INTERVAL = 60000; // 1 minute

  static unsigned long lastHeaterCheckMillis;

  if ((state != RegulatorState::REGULATING && state != RegulatorState::OVERHEATED) || extHeaterPlan == EXT_HEATER_DISABLED) {
    if (extHeaterIsOn) {
      extHeaterStop();
    }
    return;
  }

  short availablePower = (extHeaterIsOn ? EXT_HEATER_POWER : 0) + heatingPower + meterPower;
  if (extHeaterIsOn) {
    if (availablePower < EXT_HEATER_STOP_POWER) {
      if (extHeaterStop()) {
        meterPower += EXT_HEATER_POWER;
      }
      return;
    }
    if (loopStartMillis - lastHeaterCheckMillis > HEATER_CHECK_INTERVAL) {
      lastHeaterCheckMillis = loopStartMillis;
      int res = wemoPowerUsage();
      if (res >= 0 && res < EXT_HEATER_POWER_MIN) {
        eventsWrite(EXT_HEATER_EVENT, res, WEMO_OP_QUERY);
        extHeaterStop();
      } else {
        extHeaterErr(res, WEMO_OP_QUERY);
      }
    }
  } else { // heater is off
    if (availablePower > EXT_HEATER_START_POWER) {
      int res = wemoSwitch(true);
      if (extHeaterErr(res, WEMO_OP_ON))
        return;
      lastHeaterCheckMillis = loopStartMillis;
      extHeaterIsOn = true;
      meterPower -= EXT_HEATER_POWER;
    }
  }
}

bool extHeaterStop() {
  int res = wemoSwitch(false);
  if (extHeaterErr(res, WEMO_OP_OFF))
    return false;
  extHeaterIsOn = false;
  return true;
}

bool extHeaterErr(int res, byte op) {

  const byte ERR_COUNT = 2;
  static byte errorCount;

  if (res >= 0) {
    errorCount = 0;
    return false;
  }
  errorCount++;
  if (errorCount == ERR_COUNT) {
    eventsWrite(EXT_HEATER_EVENT, res, op);
    alarmSound();
    if (op == WEMO_OP_QUERY) {
      extHeaterStop();
    }
    extHeaterPlan = EXT_HEATER_DISABLED;
    extHeaterIsOn = false;
  }
  return true;
}
