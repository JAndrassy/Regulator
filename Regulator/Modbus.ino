
// Fronius Symo Hybrid SunSpec Modbus

const int METER_UID = 240;
const int MODBUS_CONNECT_ERROR = -10;
const int MODBUS_NO_RESPONSE = -11;

enum {
  MODBUS_DELAY,
  BATTERY_DATA,
  INVERTER_DATA,
  METER_DATA
};

void modbusSetup() {
  if (!requestSymoRTC()) {
    alarmCause = AlarmCause::MODBUS;
    eventsWrite(MODBUS_EVENT, -1, 0);
  }
}

boolean modbusLoop() {

  const int DELAY_MILLIS = 2000;
  const int DATASET_MILLIS = 10000;
  const byte DATASET_FAIL_COUNT = 3;
  static byte datasetState = MODBUS_DELAY;
  static unsigned long lastAction;
  static byte datasetFailCounter;

  if (loopStartMillis - lastAction > DATASET_MILLIS) { // problems getting complete data-set in time
    modbusClearData(); // for UI
    byte failDataState = datasetState;
    datasetState = BATTERY_DATA; // start again
    datasetFailCounter++;
    if (datasetFailCounter == DATASET_FAIL_COUNT) {
      eventsWrite(MODBUS_EVENT, 0, failDataState);
      alarmCause = AlarmCause::MODBUS;
      return false;
    }
  }

  switch (datasetState) {
    case MODBUS_DELAY:
      if (loopStartMillis - lastAction < DELAY_MILLIS)
        return false;
      datasetState = BATTERY_DATA;
      break;
    case BATTERY_DATA:
      if (!requestBattery())
        return false;
      datasetState = INVERTER_DATA;
      break;
    case INVERTER_DATA:
      if (!requestInverter())
        return false;
      datasetState = METER_DATA;
      break;
    case METER_DATA:
      if (!requestMeter())
        return false;
      datasetState = MODBUS_DELAY;
      datasetFailCounter = 0;
      break;
  }
  lastAction = loopStartMillis;
  return (datasetState == MODBUS_DELAY);
}

void modbusClearData() {
  b = 0;
  soc = 0;
  m = 0;
  inverterAC = 0;
}

boolean requestSymoRTC() {

  const unsigned long EPOCH_2000 = 946681200; // seconds from 1.1.1971 to 1.1.2000
  const unsigned long ZONE_DST = 2 * 3600; // Central Europe DST

  int regs[2];
  int res = modbusRequest(1, 40222, 2, regs);
  if (modbusError(res))
    return false;
  // SunSpec has seconds from 1.1.2000 UTC, TimeLib uses 'epoch' (seconds from 1.1.1970)
  setTime(EPOCH_2000 + (unsigned int) regs[0] * 65536L + (unsigned int) regs[1] + ZONE_DST);
  return true;
}

boolean requestInverter() {
  int regs[2];
  int res = modbusRequest(1, 40083, 2, regs);
  if (modbusError(res))
    return false;
  inverterAC = regs[0] * pow(10, regs[1]); // ac power * scale
  return true;
}

boolean requestMeter() {
  int regs[16];
  int res = modbusRequest(METER_UID, 40076, 16, regs);
  if (modbusError(res))
    return false;
//  voltage = regs[3] * pow(10, regs[8] + 1); // ac voltage F3 * scale
  m = -regs[11] * pow(10, regs[15]); // ac power * scale
  return true;
}

boolean requestBattery() {
  int regs[58];
  int res = modbusRequest(1, 40257, 58, regs); //MPPT reg + SF offsset
  if (modbusError(res))
    return false;
  b = (unsigned int) regs[37] * pow(10, regs[0]); // dc power * scale
  soc = regs[54] / 100; // storage register addr - mppt register addr + ChaSta offset
  switch (regs[57]) {  // charge status
    case 4:  // CHARGING
    case 6:  // HOLDING
    case 7:  // TESTING (CALIBRATION)
      break;
    default:
      b = -b;
  }
  return true;
}

boolean modbusError(int err) {

  static int modbusErrorCounter = 0;
  static int modbusErrorCode = 0;

  if (modbusErrorCode != err) {
    modbusErrorCounter = 0;
    modbusErrorCode = err;
  }
  if (err == 0)
    return false;
  modbusErrorCounter++;
  switch (modbusErrorCounter) {
    case 1:
      sprintfF(msg, F("modbus error %d"), err);
    break;
    case 10:
      sprintfF(msg, F("modbus error %d %d times"), err, modbusErrorCounter);
      eventsWrite(MODBUS_EVENT, err, 0);
      alarmCause = AlarmCause::MODBUS;
    break;
  }
  return true;
}

/*
 * return
 *   - 0 is success
 *   - negative is comm error
 *   - positive value is modbus protocol exception code
 */
int modbusRequest(byte uid, unsigned int addr, byte len, int *regs) {

  const byte CODE_IX = 7;
  const byte ERR_CODE_IX = 8;
  const byte LENGTH_IX = 8;
  const byte DATA_IX = 9;

  static NetClient modbus;

  if (!modbus.connected()) {
    modbus.stop();
    modbus.connect(symoAddress, 502);
    if (!modbus.connected()) {
      modbus.stop();
      return MODBUS_CONNECT_ERROR;
    }
    sprintfF(msg, F("modbus reconnect"));
  }
  byte request[] = {0, 1, 0, 0, 0, 6, uid, 3, (byte) (addr / 256), (byte) (addr % 256), 0, len};
  modbus.write(request, sizeof(request));

  int respDataLen = len * 2;
  byte response[max(DATA_IX, respDataLen)];
  int readLen = timedRead(modbus, response, DATA_IX);
  if (readLen < DATA_IX) {
    modbus.stop();
    return MODBUS_NO_RESPONSE;
  }
  switch (response[CODE_IX]) {
    case 3:
      break;
    case 0x83:
      return response[ERR_CODE_IX]; // 0x01, 0x02, 0x03 or 0x11
    default:
      return -3;
  }
  if (response[LENGTH_IX] != respDataLen)
    return -2;
  readLen = timedRead(modbus, response, respDataLen);
  if (readLen < respDataLen)
    return -4;
  for (int i = 0, j = 0; i < len; i++, j += 2) {
    regs[i] = response[j] * 256 + response[j + 1];
  }
  return 0;
}

size_t timedRead(Stream &is, byte buffer[], size_t len) {
  if (len == 0)
    return 0;
  size_t l = 0;
  unsigned long startMillis = millis();
  do {
    if (is.available() > 1) {
      l += is.readBytes(buffer + l, len - l);
      if (l == len)
        break;
      startMillis = millis();
    }
    delay(10); // to let bytes collect in comm buffer
  } while (millis() - startMillis < is.getTimeout());
  return l;

}

