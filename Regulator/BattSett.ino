
const unsigned int ADDR_STORCTL = 40303;
const byte REG_StorCtl_Mod = 5;
const byte REG_MinRsvPct = 7;
const byte REG_OutWRte = 12;
const byte REG_InWRte = 13;

const byte SCALE = 100;

const byte NOLIMIT_HOUR = 9;
const byte LIMIT_HOUR = 14;

// Inverter control via Modbus must be enabled.

void battSettSetup() {
  byte h = hour(now());
  battSettControl(false, h < NOLIMIT_HOUR || h >= LIMIT_HOUR);
}

void battSettLoop() {

  static boolean done = false;

  byte h = hour(now());
  if (h == NOLIMIT_HOUR || h == LIMIT_HOUR) {
    if (!done) {
      battSettControl(false, h == LIMIT_HOUR);
      done = true;
    }
  } else if (done) {
    done = false;
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

boolean battSettControl(boolean chargeControlOn, boolean dischargeControlOn) {
  int value = chargeControlOn | (dischargeControlOn << 1);
  int res = modbusWriteSingle(ADDR_STORCTL + REG_StorCtl_Mod, value);
  if (modbusError(res))
    return false;
  return true;
}

/**
 * reg is one of REG_MinRsvPct, REG_OutWRte, REG_InWRte
 * limit is 0 to 100% for REG_MinRsvPct and -100% to 100% for both WRte
 */
boolean battSettSetLimit(byte reg, int limit) {
  int res = modbusWriteSingle(ADDR_STORCTL + reg, limit * SCALE);
  if (modbusError(res))
    return false;
  return true;
}
