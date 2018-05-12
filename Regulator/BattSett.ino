
const unsigned int ADDR_STORCTL = 40303;
const byte REG_StorCtl_Mod = 5;
const byte REG_MinRsvPct = 7;
const byte REG_OutWRte = 12;
const byte REG_InWRte = 13;

const byte SCALE = 100;

// Inverter control via Modbus must be enabled.

void battSettLoop() {

  const byte EVAL_START_HOUR = 16;
  const byte LIMIT_TARGET_HOUR = 7;
  const byte PERCENT_PER_HOUR = 5;
  const byte MIN_SOC = 10;
  const byte MARGINAL_SOC = 30;

  static boolean disableDone = false;
  static boolean enableDone = false;

  // after 7 AM disable limit
  if (hourNow > LIMIT_TARGET_HOUR && hourNow < EVAL_START_HOUR) {
    if (!disableDone) {
      int res = battSettControl(false, false);
      if (res != 0) {
        eventsWrite(BATTSETT_LIMIT_EVENT, -1, res);
      }
      disableDone = true;
    }
  } else if (disableDone) {
    disableDone = false;
  }

  // after 3 PM until rest, check discharging speed
  // enable discharge limit, if SoC is lower then needed
  if (hourNow >= EVAL_START_HOUR) {
    if (!enableDone && soc > MARGINAL_SOC && (soc - MIN_SOC < PERCENT_PER_HOUR * ((24 - hourNow) + LIMIT_TARGET_HOUR))) { // %
      int res = battSettControl(false, true);
      eventsWrite(BATTSETT_LIMIT_EVENT, soc, res);
      enableDone = true;
    }
  } else if (enableDone) {
    enableDone = false;
  }
}

boolean battSettRead(FormattedPrint& out) {
  const int REGISTER_LENGTH = REG_InWRte + 1;
  int regs[REGISTER_LENGTH];
  int res = modbusRequest(1, ADDR_STORCTL, REGISTER_LENGTH, regs);
  if (modbusError(res))
    return false;
  int storCtrlMod = regs[REG_StorCtl_Mod];
  int reserve = regs[REG_MinRsvPct] / SCALE;
  int dischargeLimit = regs[REG_OutWRte] / SCALE;
  int chargeLimit = regs[REG_InWRte] / SCALE;
  out.printf(F("Charge limit %d%% is %s"), chargeLimit, bit2s(storCtrlMod, 0b01));
  out.printf(F("Discharge limit %d%% is %s"), dischargeLimit, bit2s(storCtrlMod, 0b10));
  out.printf(F("Reserve is %d%%"), reserve);
  return true;
}

const char* bit2s(int value, byte mask) {
  return (value & mask) != 0 ? "enabled" : "disabled";
}

int battSettControl(boolean chargeControlOn, boolean dischargeControlOn) {
  int value = chargeControlOn | (dischargeControlOn << 1);
  return modbusWriteSingle(ADDR_STORCTL + REG_StorCtl_Mod, value);
}

/**
 * reg is one of REG_MinRsvPct, REG_OutWRte, REG_InWRte
 * limit is 0 to 100% for REG_MinRsvPct and -100% to 100% for both WRte
 */
int battSettSetLimit(byte reg, int limit) {
  return modbusWriteSingle(ADDR_STORCTL + reg, limit * SCALE);
}
