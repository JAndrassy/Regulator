
// Fronius Symo Hybrid SunSpec Modbus

const byte METER_UID = 240;
const int MODBUS_CONNECT_ERROR = -10;
const int MODBUS_NO_RESPONSE = -11;

const byte FNC_READ_REGS = 0x03;
const byte FNC_WRITE_SINGLE = 0x06;
const byte FNC_ERR_FLAG = 0x80;

enum {
  MODBUS_DELAY,
  BATTERY_DATA,
  INVERTER_DATA,
  METER_DATA
};

NetClient modbus;

void modbusSetup() {
  if (!requestSymoRTC()) {
    alarmCause = AlarmCause::MODBUS;
    eventsWrite(MODBUS_EVENT, -1, 0);
  }
}

boolean modbusLoop() {

  const int DELAY_MILLIS = 4000;
  const int DATASET_MILLIS = 12000;
  const byte DATASET_FAIL_COUNT = 3;

  static byte datasetState = MODBUS_DELAY;
  static unsigned long datasetStart;
  static byte datasetFailCounter;

  if (datasetState != MODBUS_DELAY && loopStartMillis - datasetStart > DATASET_MILLIS) { // problems getting complete data-set in time
    modbusClearData(); // for UI
    byte failDataState = datasetState;
    datasetState = BATTERY_DATA; // start again
    datasetStart = loopStartMillis;
    datasetFailCounter++;
    if (datasetFailCounter == DATASET_FAIL_COUNT) {
      eventsWrite(MODBUS_EVENT, 0, failDataState);
      alarmCause = AlarmCause::MODBUS;
      return false;
    }
  }

  switch (datasetState) {
    case MODBUS_DELAY:
      if (loopStartMillis - datasetStart < DELAY_MILLIS)
        return false;
      datasetState = BATTERY_DATA;
      datasetStart = loopStartMillis;
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
  return (datasetState == MODBUS_DELAY);
}

void modbusClearData() {
  b = 0;
  soc = 0;
  m = 0;
  inverterAC = 0;
  voltage = 0;
}

boolean requestSymoRTC() {

  const unsigned long EPOCH_2000 = 946681200; // seconds from 1.1.1971 to 1.1.2000
  const unsigned long ZONE_DST = 2 * 3600; // Central Europe DST

  short regs[2];
  int res = modbusRequest(1, 40222, 2, regs);
  if (modbusError(res))
    return false;
  // SunSpec has seconds from 1.1.2000 UTC, TimeLib uses 'epoch' (seconds from 1.1.1970)
  setTime(EPOCH_2000 + (unsigned short) regs[0] * 65536L + (unsigned short) regs[1] + ZONE_DST);
  return true;
}

boolean requestInverter() {
  short regs[2];
  int res = modbusRequest(1, 40083, 2, regs);
  if (modbusError(res))
    return false;
  inverterAC = regs[0] * pow(10, regs[1]); // ac power * scale
  return true;
}

boolean requestMeter() {
  short regs[16];
  int res = modbusRequest(METER_UID, 40076, 16, regs);
  if (modbusError(res))
    return false;
  voltage = regs[3] * pow(10, regs[8]); // ac voltage F3 * scale
  m = -regs[11] * pow(10, regs[15]); // ac power * scale
  return true;
}

boolean requestBattery() {
  short regs[58];
  int res = modbusRequest(1, 40257, 58, regs); //MPPT reg + SF offsset
  if (modbusError(res))
    return false;
  b = (unsigned short) regs[37] * pow(10, regs[0]); // dc power * scale
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

  static byte modbusErrorCounter = 0;
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
      msg.printf(F("modbus error %d"), err);
    break;
    case 10:
      msg.printf(F("modbus error %d %d times"), err, modbusErrorCounter);
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
 *   - error 4 is SLAVE_DEVICE_FAILURE. Check if 'Inverter control via Modbus' is enabled.
 */
int modbusRequest(byte uid, unsigned int addr, byte len, short *regs) {

  const byte CODE_IX = 7;
  const byte ERR_CODE_IX = 8;
  const byte LENGTH_IX = 8;
  const byte DATA_IX = 9;

  int err = modbusConnection();
  if (err != 0)
    return err;

  byte request[] = {0, 1, 0, 0, 0, 6, uid, FNC_READ_REGS, (byte) (addr / 256), (byte) (addr % 256), 0, len};
  modbus.write(request, sizeof(request));

  int respDataLen = len * 2;
  byte response[max((int) DATA_IX, respDataLen)];
  int readLen = modbus.readBytes(response, DATA_IX);
  if (readLen < DATA_IX) {
    modbus.stop();
    return MODBUS_NO_RESPONSE;
  }
  switch (response[CODE_IX]) {
    case FNC_READ_REGS:
      break;
    case (FNC_ERR_FLAG | FNC_READ_REGS):
      return response[ERR_CODE_IX]; // 0x01, 0x02, 0x03 or 0x11
    default:
      return -3;
  }
  if (response[LENGTH_IX] != respDataLen)
    return -2;
  readLen = modbus.readBytes(response, respDataLen);
  if (readLen < respDataLen)
    return -4;
  for (int i = 0, j = 0; i < len; i++, j += 2) {
    regs[i] = response[j] * 256 + response[j + 1];
  }
  return 0;
}

int modbusWriteSingle(unsigned int address, int val) {

  const byte CODE_IX = 7;
  const byte ERR_CODE_IX = 8;
  const byte RESPONSE_LENGTH = 9;

  int err = modbusConnection();
  if (err != 0)
    return err;

  byte req[] = { 0, 1, 0, 0, 0, 6, 1, FNC_WRITE_SINGLE, // header
        (byte) (address / 256), (byte) (address % 256),
        (byte) (val / 256), (byte) (val % 256)};

  modbus.write(req, sizeof(req));

  byte response[RESPONSE_LENGTH];
  int readLen = modbus.readBytes(response, RESPONSE_LENGTH);
  if (readLen < RESPONSE_LENGTH) {
    modbus.stop();
    return MODBUS_NO_RESPONSE;
  }
  switch (response[CODE_IX]) {
    case FNC_WRITE_SINGLE:
      break;
    case (FNC_ERR_FLAG | FNC_WRITE_SINGLE):
      return response[ERR_CODE_IX]; // 0x01, 0x02, 0x03, 0x04 or 0x11
    default:
      return -3;
  }
  while (modbus.read() != -1); // 4 more bytes address and reg value
  return 0;
}

int modbusConnection() {
  if (!modbus.connected()) {
    modbus.stop();
    if (!modbus.connect(symoAddress, 502))
      return MODBUS_CONNECT_ERROR;
    modbus.setTimeout(2000);
    msg.print(F(" modbus reconnect"));
  }
  return 0;
}

