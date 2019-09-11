
#ifdef ARDUINO_SAM_ZERO
#define Serial SerialUSB
//#include <SAMD_AnalogCorrection.h>
#endif

#define TRIAC

#ifdef TRIAC
#ifdef ARDUINO_ARCH_SAMD
#include <TriacLib.h>
#elif defined(ARDUINO_ARCH_AVR)
#include <TriacDimmer.h>
#endif
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#elif defined(ARDUINO_AVR_UNO_WIFI_DEV_ED)
#include <WiFiLink.h>
#include <UnoWiFiDevEdSerial1.h>
#else
#include <Ethernet.h>
#include <SD.h>
#include <ArduinoOTA.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
#endif

#if defined(ethernet_h_) || defined(UIPETHERNET_H)
#define NetServer EthernetServer
#define NetClient EthernetClient
#else
#define NetServer WiFiServer
#define NetClient WiFiClient
#endif

#ifdef ESP8266
const byte MAIN_RELAY_PIN = 16; //GBS_D2_io16;
// GBS_D3 and D4 I2C
const byte BYPASS_PIN = 0; //GBS_D5_io0_PULLUP;
const byte TONE_PIN = 2; //GBS_D6_io2_PULLUP;
const byte PWM_PIN = 14; // GBS_D7_io14;
const byte TEMSENS_PIN = A0;
const byte BALBOA_RELAY_PIN = 10; // GBS_A2_io15_PULLDOWN; // jumper wire from pin 10 to unused A2
const byte VALVES_RELAY_PIN = 3; // GBS_A3_io3_TX0; // jumper wire from pin TX to unused A3
#else

const byte MAIN_RELAY_PIN = 2;
const byte TONE_PIN = 3;
const byte SD_SS_PIN = 4; // SD card SS
#ifdef TRIAC
#ifdef ARDUINO_ARCH_SAMD
const byte BYPASS_RELAY_PIN = 5;
const byte TRIAC_PIN = 6;  // TCC0 WO pin for TriacLib
// pin 7 zero-crossing external interrupt for TriacLib
#elif defined(PROBADIO)
const byte TRIAC_PIN = 5; // TIMER1 OC1A for TriacDimmer library
// pin 6 TIMER1 ICP1 for TriacDimmer library
const byte BYPASS_RELAY_PIN = 7;
#endif
const byte ZC_EI_PIN = TRIAC_PIN + 1; // it is the second pin on the Grove connector to Dimmer
const byte BUTTON_PIN = 11;
#else
const byte BYPASS_RELAY_PIN = 5;
const byte PWM_PIN = 6;
const byte BUTTON_PIN = 7;
#endif
const byte LEDBAR_DATA_PIN = 8;
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector
const byte NET_SS_PIN = 10;
//pin 10-13 SPI (Ethernet, SD)
const byte TEMPSENS_PIN = A0;
const byte ELSENS_PIN = A1;
const byte BALBOA_RELAY_PIN = A2;
const byte VALVES_RELAY_PIN = A3;
//pin A4, A5 is I2C on AVR (ADC, on Uno Wifi ESP8266 over I2C SC)
#endif

const int START_BEEP = 4186;
const int BEEP_1 = 4186;
const int BEEP_2 = 4699;

NetServer telnetServer(2323);

#define I2C_ADDR_ADC121         0x50
#ifdef I2C_ADDR_ADC121
#include <Wire.h>
const byte REG_ADDR_RESULT = 0x00;
const byte REG_ADDR_CONFIG = 0x02;
#endif

const char version[] = "build "  __DATE__ " " __TIME__;

void setup() {
  Serial.begin(115200);

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

#ifdef I2C_ADDR_ADC121
  Wire.begin();
  Wire.setClock(400000);
#endif

#ifdef TRIAC
#ifdef ARDUINO_ARCH_SAMD
  Triac::setup(ZC_EI_PIN, TRIAC_PIN);
#elif defined(ARDUINO_ARCH_AVR)
  TriacDimmer::begin();
#endif
#endif

  pinMode(BYPASS_RELAY_PIN, OUTPUT);
  pinMode(MAIN_RELAY_PIN, OUTPUT);
  pinMode(VALVES_RELAY_PIN, OUTPUT);
  pinMode(BALBOA_RELAY_PIN, OUTPUT);

  beep();
  Serial.println("START");

#ifdef __SD_H__
  pinMode(NET_SS_PIN, OUTPUT);
  digitalWrite(NET_SS_PIN, HIGH);
  if (SD.begin(SD_SS_PIN)) {
    Serial.println(F("SD card initialized"));
  }
#endif

  #ifdef ESP8266
  ArduinoOTA.begin();
  WiFi.mode(WIFI_STA);
  IPAddress ip(192, 168, 1, 8);
  IPAddress gw(192, 168, 1, 1);
  IPAddress sn(255, 255, 255, 0);
  WiFi.config(ip, gw, sn);
  WiFi.begin("", "");
  Serial.println(WiFi.waitForConnectResult());
  Serial.println(WiFi.localIP());
#elif defined(ethernet_h_) || defined(UIPETHERNET_H)
  Ethernet.init(10);
  IPAddress ip(192, 168, 1, 8);
  Ethernet.begin(mac, ip);
#else
  Serial1.begin(115200);
  Serial1.resetESP();
  WiFi.init(&Serial1);
  delay(3000);
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
  }
#endif

#if defined(ARDUINO_AVR_ATMEGA1284)
  SDStorage.setUpdateFileName("FIRMWARE.BIN");
  SDStorage.clear(); // AVR SD bootloaders don't delete the update file
  ArduinoOTA.begin(ip, "regulator", "password", SDStorage);
#else
  ArduinoOTA.begin(ip, "Arduino", "password", InternalStorage);
#endif

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)
  analogWriteResolution(10);
  analogReadResolution(12);
#endif
#ifdef ARDUINO_ARCH_SAMD
//  analogReadCorrection(11, 2054);
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
    client = telnetServer.available();
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
    if (v == -1) {
      digitalWrite(MAIN_RELAY_PIN, LOW);
    } else if (v == -2) {
      digitalWrite(VALVES_RELAY_PIN, LOW);
      digitalWrite(MAIN_RELAY_PIN, HIGH);
    } else if (v == -3) {
      digitalWrite(VALVES_RELAY_PIN, LOW);
    } else if (v == -4) {
      digitalWrite(MAIN_RELAY_PIN, LOW);
      digitalWrite(VALVES_RELAY_PIN, HIGH);
    } else if (v == 1024) {
#ifdef TRIAC
      pilotTriacPeriod(0);
#else
      analogWrite(PWM_PIN, 0);
#endif
      digitalWrite(MAIN_RELAY_PIN, HIGH);
      digitalWrite(BYPASS_RELAY_PIN, HIGH);
    } else if (v == 0) {
#ifdef TRIAC
      pilotTriacPeriod(0);
#else
      analogWrite(PWM_PIN, 0);
#endif
      digitalWrite(BYPASS_RELAY_PIN, LOW);
    } else {
      digitalWrite(MAIN_RELAY_PIN, HIGH);
#ifdef TRIAC
      pilotTriacPeriod((float) v / 10000);
#else
      analogWrite(PWM_PIN, v);
#endif
      digitalWrite(BYPASS_RELAY_PIN, LOW);
    }
    v = readElSens();
    client.println(v);
  } else if (c == 'T') {
//    client.println(analogRead(TEMPSENS_PIN));
    const short size = 400;
    unsigned short ts[size];
    unsigned short data[size];
    unsigned long start = micros();
    for (int i = 0; i < size; i++) {
      do {
        data[i] = elsensAnalogRead();
      } while (data[i] < 100);
      ts[i] = micros() - start;
    }
    for (int i = 0; i < size; i++) {
      client.print(ts[i]);
      client.print('\t');
      client.println(data[i]);
    }
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
#ifdef ARDUINO_ARCH_SAMD
  Triac::setPeriod(p);
#elif defined(ARDUINO_ARCH_AVR)
  TriacDimmer::setBrightness(TRIAC_PIN, p);
#endif
}

int readElSens() {

  // sample AC
  unsigned long long sum = 0;
  int n = 0;
  unsigned long start_time = millis();
  while (millis() - start_time < 100) { // in 200 ms measures 10 50Hz AC oscillations
    unsigned long v = elsensAnalogRead();
    sum += v * v;
    n++;
  }
  return sqrt((double) sum / (n / 2)); // RMS, but half of the values are zeros for removed negative voltages
}

unsigned short elsensAnalogRead() {
#ifdef I2C_ADDR_ADC121
  Wire.requestFrom(I2C_ADDR_ADC121, 2);
  byte buff[2];
  Wire.readBytes(buff, 2);
  return (buff[0] << 8) | buff[1];
#elif defined(analogReadFast_H)
  return analogReadFast(ELSENS_PIN);
#else
  return analogRead(ELSENS_PIN);
#endif
}

void beeperTone(int freq, uint32_t time) {
#ifdef ARDUINO_ARCH_NRF5
  int d = (1000 * 1000 / 2 / freq) - 20;
  bool s = true;
  uint32_t t = millis();
  while (millis() - t < time) {
    digitalWrite(TONE_PIN, s);
    s = !s;
    delayMicroseconds(d);
  }
  digitalWrite(TONE_PIN, LOW);
#else
  tone(TONE_PIN, freq);
  delay(time);
  noTone(TONE_PIN);
#endif
}

void beep() {
  Serial.println("beep");
  pinMode(TONE_PIN, OUTPUT);
  beeperTone(BEEP_1, 200);
  pinMode(TONE_PIN, INPUT); // to reduce noise from amplifier
}
