
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
uint8_t requestId = 0;

void modbusSetup() {

  const unsigned long GET_TIME_ATTEMPTS_TIMEOUT = 5000;

  unsigned long startMillis = millis();
  while (millis() - startMillis < GET_TIME_ATTEMPTS_TIMEOUT) {
    if (requestSymoRTC())
      return;
    delay(500);
  }
  alarmCause = AlarmCause::MODBUS; // time was not retrieved
}

boolean modbusLoop() {

  const int DELAY_MILLIS = 8000;

  static byte datasetState = MODBUS_DELAY;
  static unsigned long datasetStart;

  switch (datasetState) {
    case MODBUS_DELAY:
      if (loopStartMillis - datasetStart < DELAY_MILLIS)
        return false;
      datasetState = BATTERY_DATA;
      datasetStart = loopStartMillis;
      break;
    case BATTERY_DATA:
      if (!requestBattery()) {
        datasetState = MODBUS_DELAY;
      } else {
        datasetState = INVERTER_DATA;
      }
      break;
    case INVERTER_DATA:
      if (!requestInverter()) {
        datasetState = MODBUS_DELAY;
      } else {
        datasetState = METER_DATA;
      }
      break;
    case METER_DATA:
      datasetState = MODBUS_DELAY;
      return requestMeter();
  }
  return false;
}

void modbusClearData() {
  modbus.stop();
  pvChargingPower = 0;
  pvSOC = 0;
  meterPower = 0;
  meterPowerPhaseA = 0;
  meterPowerPhaseB = 0;
  meterPowerPhaseC = 0;
  inverterAC = 0;
  voltage = 0;
}

boolean requestSymoRTC() {

  short regs[2];
  int res = modbusRequest(1, 40222, 2, regs);
  if (modbusError(res))
    return false;
  // SunSpec has seconds from 1.1.2000 UTC, TimeLib uses 'epoch' (seconds from 1.1.1970)
  setTime(SECS_YR_2000 + (unsigned short) regs[0] * 65536L + (unsigned short) regs[1] + SECS_PER_HOUR); // Europe DST
  int m = month();
  if (m > 10 || m < 3) {
    setTime(now() - SECS_PER_HOUR);
  } else if (m == 3) {
    int d = 31 - ((((5 * year()) / 4) + 4) % 7);
    if (day() < d) {
      setTime(now() - SECS_PER_HOUR);
    }
  } else if (m == 10) {
    int d = 31 - ((((5 * year()) / 4) + 1) % 7);
    if (day() >= d) {
      setTime(now() - SECS_PER_HOUR);
    }
  }
  hourNow = hour();
#ifdef ARDUINO_ARCH_SAMD
  rtc.setEpoch(now());
#endif
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
  float sf = pow(10, regs[15]);
  meterPower = -regs[11] * sf; // ac power * scale
  meterPowerPhaseA = regs[12] * sf;
  meterPowerPhaseB = regs[13] * sf;
  meterPowerPhaseC = regs[14] * sf;
  return true;
}

boolean requestBattery() {
  short regs[58];
  int res = modbusRequest(1, 40257, 58, regs); //MPPT reg + SF offsset
  if (modbusError(res))
    return false;
  short pvProduction = (unsigned short) regs[17] * pow(10, regs[0]); // mppt 1 dc power * scale
  pvChargingPower = (unsigned short) regs[37] * pow(10, regs[0]); // dc power * scale
  pvSOC = regs[54] / 100; // storage register addr - mppt register addr + ChaSta offset
  pvBattCalib = false;
  switch (regs[57]) {  // charge status
    case 2:  // EMPTY
        if (pvProduction < inverterAC) {
          pvChargingPower = -pvChargingPower;
        }
      break;
    case 7:  // TESTING (CALIBRATION)
      pvBattCalib = true;
      break;
    case 3:  // DISCHARGING
    case 5:  // FULL
      pvChargingPower = -pvChargingPower;
  }
  return true;
}

boolean modbusError(int err) {

  const byte ERROR_COUNT_ALARM = 5;

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
      msg.printf(F(" MB_error_%d"), err);
    break;
    case ERROR_COUNT_ALARM:
      msg.printf(F(" MB_error_%d_%d_times"), err, modbusErrorCounter);
      if (state != RegulatorState::ALARM) {
        eventsWrite(MODBUS_EVENT, err, 0);
        alarmCause = AlarmCause::MODBUS;
      }
      modbus.stop();
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

  byte request[] = {0, requestId++, 0, 0, 0, 6, uid, FNC_READ_REGS, (byte) (addr / 256), (byte) (addr % 256), 0, len};
  modbus.write(request, sizeof(request));
  modbus.flush();

  int respDataLen = len * 2;
  byte response[max((int) DATA_IX, respDataLen)];

  while (true) {
    int readLen = modbus.readBytes(response, DATA_IX);
    if (readLen < DATA_IX) {
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
    if ((uint8_t)(requestId - 1) != response[1]) {
      msg.printf(F(" %d!=%d"), requestId - 1, (int) response[1]);
      int l = response[LENGTH_IX];
      while (l > 0 && modbus.read() != -1) {
        l--;
      }
      continue; // while
    }
    readLen = modbus.readBytes(response, respDataLen);
    if (readLen < respDataLen)
      return -4;
    break;
  }
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

  byte req[] = {0, requestId++, 0, 0, 0, 6, 1, FNC_WRITE_SINGLE, // header
        (byte) (address / 256), (byte) (address % 256),
        (byte) (val / 256), (byte) (val % 256)};

  modbus.write(req, sizeof(req));
  modbus.flush();

  byte response[RESPONSE_LENGTH];
  int readLen = modbus.readBytes(response, RESPONSE_LENGTH);
  if (readLen < RESPONSE_LENGTH)
    return MODBUS_NO_RESPONSE;
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
  while (modbus.read() != -1); // clean the buffer
  if (!modbus.connected()) {
    modbus.stop();
    if (!modbus.connect(symoAddress, 502))
      return MODBUS_CONNECT_ERROR;
    modbus.setTimeout(3000);
    msg.print(F(" MB_reconnect"));
  }
  return 0;
}

