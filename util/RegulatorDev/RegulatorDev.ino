
#include <TriacLib.h>

#include <Ethernet.h>
#include <SD.h>
#define NO_OTA_PORT
#include <ArduinoOTA.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

#if defined(ethernet_h_) || defined(UIPETHERNET_H)
#define NetServer EthernetServer
#define NetClient EthernetClient
#else
#define NetServer WiFiServer
#define NetClient WiFiClient
#endif

#ifdef ARDUINO_SAMD_MKRZERO // on MKR Connector Carrier

const byte TONE_PIN = 0;
const byte BALBOA_RELAY_PIN = 1;
const byte VALVES_RELAY_PIN = 2;
const byte MAIN_RELAY_PIN = 3;
const byte BYPASS_RELAY_PIN = 4;
const byte ZC_EI_PIN = 5;  // on same connector with 6
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib
const byte NET_SS_PIN = 7;  // not on Carrier
// SPI 8, 9, 10 not on Carrier
// TWI 12, 11
const byte TEMPSENS_PIN = A0;
const byte ELSENS_PIN = A1;
const byte BUTTON_PIN = A4;
const byte LEDBAR_DATA_PIN = 13; // connector labeled Serial (it is for Serial1)
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector

const byte SD_SS_PIN = SDCARD_SS_PIN; // internal pin of MKR ZERO

#else

const byte TONE_PIN = 2;
#ifdef ARDUINO_SAMD_ZERO
const byte MAIN_RELAY_PIN = 3;
const byte SD_SS_PIN = 4;  // Ethernet shield
const byte ZC_EI_PIN = 5;  // on one Grove connector with TRIAC_PIN
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib
const byte NET_SS_PIN = 10; // Ethernet shield
#elif defined(ARDUINO_AVR_ATMEGA1284)
const byte ZC_EI_PIN = 3; // INT1 pin. on one Grove connector with TRIAC_PIN
const byte TRIAC_PIN = 4; // TIMER1 OC2A
const byte MAIN_RELAY_PIN = 5;
const byte BYPASS_RELAY_PIN = 6;
const byte SD_SS_PIN = 10;  // Adafruit SD card adapter directly on pins 10 to 13
//pin 10-13 SPI (Ethernet, SD)
const byte NET_SS_PIN = A5; // is close to SPI header
#endif

// A0 free
const byte ELSENS_PIN = A1;
const byte BALBOA_RELAY_PIN = A2;
const byte VALVES_RELAY_PIN = A3;
#endif

const int BEEP_1 = 2637;
const int BEEP_2 = 2794;

NetServer telnetServer(2323);

//#define I2C_ADDR_ADC121         0x50
#ifdef I2C_ADDR_ADC121
#include <Wire.h>
const byte REG_ADDR_RESULT = 0x00;
const byte REG_ADDR_CONFIG = 0x02;
#elif ARDUINO_ARCH_SAMD
#include <avdweb_AnalogReadFast.h>
#endif

const char version[] = "build "  __DATE__ " " __TIME__;

bool mainRelayOn;

void setup() {
  Serial.begin(115200);

  pinMode(SD_SS_PIN, OUTPUT);
  digitalWrite(SD_SS_PIN, HIGH);

#ifdef I2C_ADDR_ADC121
  Wire.begin();
  Wire.setClock(400000);
#endif

  Triac::setup(ZC_EI_PIN, TRIAC_PIN);

  pinMode(BYPASS_RELAY_PIN, OUTPUT);
  pinMode(MAIN_RELAY_PIN, OUTPUT);
  pinMode(VALVES_RELAY_PIN, OUTPUT);
  pinMode(BALBOA_RELAY_PIN, OUTPUT);

  beep();
  Serial.println("START");
  Serial.println(version);

#ifdef __SD_H__
  pinMode(NET_SS_PIN, OUTPUT);
  digitalWrite(NET_SS_PIN, HIGH);
  if (SD.begin(SD_SS_PIN)) {
    Serial.println(F("SD card initialized"));
#if defined(ARDUINO_AVR_ATMEGA1284)
    SDStorage.setUpdateFileName("FIRMWARE.BIN");
    SDStorage.clear(); // AVR SD bootloaders don't delete the update file
#endif
  }
#endif

  Ethernet.init(NET_SS_PIN);
  IPAddress ip(192, 168, 1, 6);
  Ethernet.begin(mac, ip);

#if defined(ARDUINO_AVR_ATMEGA1284)
  ArduinoOTA.begin(ip, "regulator", "password", SDStorage);
#else
  ArduinoOTA.begin(ip, "regulator", "password", InternalStorage);
#endif

  telnetServer.begin();

#ifdef I2C_ADDR_ADC121
  Wire.beginTransmission(I2C_ADDR_ADC121);        // transmit to device
  Wire.write(REG_ADDR_CONFIG);                // Configuration Register
  Wire.write(REG_ADDR_RESULT);
  Wire.endTransmission();

  Wire.beginTransmission(I2C_ADDR_ADC121);        // transmit to device
  Wire.write(REG_ADDR_RESULT);                // get result
  Wire.endTransmission();
#endif
  beep();

  digitalWrite(BYPASS_RELAY_PIN, LOW);
  digitalWrite(MAIN_RELAY_PIN, LOW);
  Serial.println("READY");
}

String s = "";
NetClient client;

void loop() {

  ArduinoOTA.handle();

  if (!client) {
    client = telnetServer.accept();
    if (!client)
      return;
  }
  if (!client.connected()) {
    client.stop();
    return;
  }
//  static unsigned long previousMillis;
//  unsigned long currentMillis = millis();
//  if (currentMillis - previousMillis > 5000) {
//    previousMillis = currentMillis;
//    client.println(".");
//  }

  if (!client.available())
    return;

  char c = client.read();
  if (c == '\n') {
    long v = s.toInt();
    s = "";

    if (v == -1) { // main relay off
      if (mainRelayOn) {
        Triac::waitZeroCrossing();
        digitalWrite(MAIN_RELAY_PIN, LOW);
        mainRelayOn = false;
      }
    } else if (v == -2) { // main relay on
      digitalWrite(VALVES_RELAY_PIN, LOW);
      if (!mainRelayOn) {
        digitalWrite(MAIN_RELAY_PIN, HIGH);
        mainRelayOn = true;
      }
    } else if (v == -4) { // valves back on
      if (mainRelayOn) {
        Triac::waitZeroCrossing();
        digitalWrite(MAIN_RELAY_PIN, LOW);
        mainRelayOn = false;
      }
      digitalWrite(VALVES_RELAY_PIN, HIGH);

    } else if (v == -5) { // bypass on
      pilotTriacPeriod(0);
      if (!mainRelayOn) {
        Triac::waitZeroCrossing();
        digitalWrite(MAIN_RELAY_PIN, HIGH);
        mainRelayOn = true;
        delay(1000);
      }
      Triac::waitZeroCrossing();
      digitalWrite(BYPASS_RELAY_PIN, HIGH);
    } else if (v == 0) {
      pilotTriacPeriod(0);
      Triac::waitZeroCrossing();
      digitalWrite(BYPASS_RELAY_PIN, LOW);
    } else {
      if (!mainRelayOn) {
        digitalWrite(MAIN_RELAY_PIN, HIGH);
        mainRelayOn = true;
        delay(1000);
      }
      pilotTriacPeriod((float) v / 10000);
      Triac::waitZeroCrossing();
      digitalWrite(BYPASS_RELAY_PIN, LOW);
    }
    v = readElSens();
    client.println(v);
  } else if (c == 'T') {
    sampleAC(0);
  } else if (c == 'R') {
    Serial.println("RESET");
    client.stop();
    while(true);
  } else if (c == 'V') {
    client.println(version);
  } else if (c == 'C') {
    client.println("bye bye");
    while (client.read() != -1)
      ;
    client.stop();
  } else {
    s += c;
  }
}

void pilotTriacPeriod(float p) {
  Triac::setPeriod(p);
}

int readElSens() {

  // sample AC
  unsigned long long sum = 0;
  int n = 0;
  unsigned long start_time = millis();
  while (millis() - start_time < 200) { // in 200 ms measures 10 50Hz AC oscillations
    long v = (short) elsensAnalogRead() - 512;
    sum += v * v;
    n++;
  }
  return sqrt((double) sum / n) * 10;
}

unsigned short elsensAnalogRead() {
#ifdef I2C_ADDR_ADC121
  Wire.requestFrom(I2C_ADDR_ADC121, 2);
  byte buff[2];
  Wire.readBytes(buff, 2);
  return (buff[0] << 8) | buff[1];
#elif ARDUINO_ARCH_SAMD
  return analogReadFast(ELSENS_PIN);
#else
  return analogRead(ELSENS_PIN);
#endif
}

void beeperTone(int freq, uint32_t time) {
  tone(TONE_PIN, freq);
  delay(time);
  noTone(TONE_PIN);
}

void beep() {
  Serial.println("beep");
  beeperTone(BEEP_1, 200);
}

void sampleAC(unsigned long start) {
  const short size = 400;
  unsigned long ts[size];
  unsigned short data[size];
  Triac::zeroCrossingFlag = false;
  for (int i = 0; i < size; i++) {
//    if (Triac::zeroCrossingFlag) {
//      Triac::zeroCrossingFlag = false;
//      data[i] = 1023;
//    } else {
      data[i] = elsensAnalogRead();
//    }
    ts[i] = micros() - start;
  }
  for (int i = 0; i < size; i++) {
    client.print(ts[i]);
    client.print('\t');
    client.println(data[i]);
  }
}
