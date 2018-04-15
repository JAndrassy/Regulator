
const char uri[] PROGMEM = "/servicecgi-bin/suspend_battery_calibration/?method=save";
const char data[] PROGMEM = "{\"suspension_time_seconds\":10800}"; // 3 hours

void susCalibLoop() {

  const int SUSCALIB_HOUR = 9;
  static boolean done = false;

  if (done || hour(now()) != SUSCALIB_HOUR)
    return;

  int st = -1;
  NetClient client;

  if (client.connect(symoAddress, 80)) {
    st = 0;

    char buff[50];
    strcpy_P(buff, uri);

    client.print(F("POST "));
    client.print(buff);
    client.println(F(" HTTP/1.1"));
    client.print(F("Host: "));
    client.println(symoAddress);
    client.println(F("Connection: close"));
    client.print(F("Content-Length: "));
    client.println(strlen_P(data));
    client.print(F("Authorization: Digest username=\"service\",realm=\"W\",nonce=\"ffcb243c6f951a5bab771e9d3b8f81d6\",uri=\""));
    client.print(buff);
    client.println(F("\",response=\"" SUSCALIB_DIGEST_RESPONSE "\""));
    client.println();
    strcpy_P(buff, data);
    client.print(buff);

    client.setTimeout(2000);
    client.readBytesUntil(' ', buff, 50); // HTTP/1.1
    client.readBytesUntil(' ', buff, 50); // status code
    st = atoi(buff);
    while (client.read() != -1);
    client.stop();
  }
  eventsWrite(SUSPEND_CALIBRATION_EVENT, st, 0);
  done = true;
}
