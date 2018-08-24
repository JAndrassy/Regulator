
const char uri[] PROGMEM = "/servicecgi-bin/suspend_battery_calibration/?method=save"; // is used twice
const char data[] PROGMEM = "{\"suspension_time_seconds\":10800}"; // 3 hours

void susCalibLoop() {

  const byte SUSCALIB_HOUR = 9;
  static boolean done = false;

  if (hourNow != SUSCALIB_HOUR) {
    if (done) {
      done = false;
    }
    return;
  }
  if (done)
    return;

  int st = -1;
  NetClient client;

  if (client.connect(symoAddress, 80)) {
    st = 0;

    char buff[64];
    BufferedPrint bp(client, buff, sizeof(buff));

    bp.print(F("POST "));
    bp.print((__FlashStringHelper*) uri); // FlashStringHelper cast to call the PROGMEM version of print
    bp.println(F(" HTTP/1.1"));
    bp.print(F("Host: "));
    bp.println(symoAddress);
    bp.println(F("Connection: close"));
    bp.print(F("Content-Length: "));
    bp.println(strlen_P(data));
    bp.print(F("Authorization: Digest username=\"service\",realm=\"W\",nonce=\"ffcb243c6f951a5bab771e9d3b8f81d6\",uri=\""));
    bp.print((__FlashStringHelper*) uri);
    bp.println(F("\",response=\"" SUSCALIB_DIGEST_RESPONSE "\"")); // SUSCALIB_DIGEST_RESPONSE is in secrets.h
    bp.println();
    bp.print((__FlashStringHelper*) data);
    bp.flush();

    client.setTimeout(5000);
    client.readBytesUntil(' ', buff, sizeof(buff)); // HTTP/1.1
    client.readBytesUntil(' ', buff, sizeof(buff)); // status code
    st = atoi(buff);
    client.setTimeout(10);
    while (client.readBytes(buff, sizeof(buff)) != 0); // read the rest
    client.stop();
  }
  eventsWrite(SUSPEND_CALIBRATION_EVENT, st, 0);
  done = true;
}
