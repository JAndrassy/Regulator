#include <MD5.h> // https://github.com/tzikis/ArduinoMD5

#define FSH_P const __FlashStringHelper*

const char uri[] PROGMEM = "/servicecgi-bin/suspend_battery_calibration/?method=save"; // is used twice
const char data[] PROGMEM = "{\"suspension_time_seconds\":10800}"; // 3 hours
const char USERNAME_P[] PROGMEM = "service";
const char REALM_P[] PROGMEM = "Webinterface area";

void susCalibLoop() {

  const byte SUSCALIB_HOUR = 9;
  static boolean done = false;
  static char nonce[64] = "";

  if (hourNow != SUSCALIB_HOUR) {
    if (done) {
      done = false;
      nonce[0] = 0;
    }
    return;
  }
  if (done)
    return;
  if (pvBattCalib) // over night calibration in progress
    return;

  char buff[128];
  char authResponse[33] = "";
  if (strlen(nonce)) {
    char ha2[33];
    strcpy_P(buff, PSTR("POST:")); // build HA2
    strcat_P(buff, uri);
    md5HashHex(ha2, buff);  // HA2 is HEX md5 of method:URI
    strcpy_P(buff, USERNAME_P); // build HA1
    strcat(buff, ":");
    strcat_P(buff, REALM_P);
    strcat(buff, ":");
    strcat(buff, SECRET_SYMO_SERVICE_PASS);
    md5HashHex(buff, buff); // HA1 starts the response string
    strcat(buff, ":");
    strcat(buff, nonce);
    strcat(buff, ":");
    strcat(buff, ha2);
    md5HashHex(authResponse, buff);
  }

  int st = -1;
  NetClient client;
#if defined(ETHERNET) && !defined(UIP_CONNECT_TIMEOUT)
  client.setConnectionTimeout(3000);
#endif
  if (client.connect(symoAddress, 80)) {
    st = 0;

    BufferedPrint bp(client, buff, sizeof(buff));

    bp.print(F("POST "));
    bp.print((FSH_P) uri);
    bp.println(F(" HTTP/1.1"));
    bp.print(F("Host: "));
    bp.println(symoAddress);
    bp.println(F("Connection: close"));
    bp.print(F("Content-Length: "));
    bp.println(strlen_P(data));
    if (strlen(authResponse)) {
      bp.print(F("Authorization: Digest username=\""));
      bp.print((FSH_P) USERNAME_P);
      bp.print(F("\",realm=\""));
      bp.print((FSH_P) REALM_P);
      bp.print(F("\",nonce=\""));
      bp.print(nonce);
      bp.print(F("\",uri=\""));
      bp.print((FSH_P) uri);
      bp.print(F("\",response=\""));
      bp.print(authResponse);
      bp.println('"');
    }
    bp.println();
    bp.print((FSH_P) data);
    bp.flush();

    client.setTimeout(8000);
    if (client.findUntil((char*) "HTTP/1.1 ", (char*) "\n")) {
      int l = client.readBytesUntil(' ', buff, sizeof(buff)); // status code
      if (l >= 0) {
        buff[l] = 0;
        if (strlen(buff) == 3) {
          st = atoi(buff);
        } else {
          st = -4;
        }
      } else {
        st = -3;
      }
    } else {
      st = -2;
    }
    if (st == 401) {
      nonce[0] = 0;
      if (client.find((char*) "nonce=\"")) {
        int l = client.readBytesUntil('"', nonce, sizeof(nonce));
        if (l) {
          nonce[l] = 0;
        } else {
          st = -6;
        }
      } else {
        st = -5;
      }
    }
    client.setTimeout(10);
    while (client.readBytes(buff, sizeof(buff)) != 0); // read the rest
    client.stop();
  }
 if (st != 401) {
  eventsWrite(SUSPEND_CALIBRATION_EVENT, st, 0);
  done = true;
 }
}

void md5HashHex(char* dest, const char* str) {

  static const char hexDigits[] = "0123456789abcdef";

  int len = 16;
  byte hash[len];

  MD5_CTX context;
  MD5::MD5Init(&context);
  MD5::MD5Update(&context, str, strlen(str));
  MD5::MD5Final(hash, &context);

  for (int i = 0; i < len; i++) {
    dest[i * 2] = hexDigits[hash[i] >> 4];
    dest[(i * 2) + 1] = hexDigits[hash[i] & 0x0F];
  }
  dest[len * 2] = '\0';
}
