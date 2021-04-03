
void consumptionMeterLoop() {

  const byte CONSUMPTION_METER_UID = 241;

  short regs[5];
  int err = modbusRequest(CONSUMPTION_METER_UID, 40087, 5, regs);
  if (err == 0) {
    measuredPower = -regs[0] * pow(10, regs[4]); // ac power * scale
  }
}

