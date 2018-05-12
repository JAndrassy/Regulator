
const char uri[] PROGMEM = "/servicecgi-bin/suspend_battery_calibration/?method=save";
const char data[] PROGMEM = "{\"suspension_time_seconds\":10800}"; // 3 hours

void susCalibLoop() {

  const byte SUSCALIB_HOUR = 9;
  static boolean done = false;

  if (done || hourNow != SUSCALIB_HOUR)
    return;

  int st = -1;
  NetClient client;

  if (client.connect(symoAddress, 80)) {
    st = 0;

    char buff[64];
    strcpy_P(buff, uri);

    char printBuff[64];
    BufferedPrint bp(client, printBuff, sizeof(printBuff));

    bp.printf(F("POST %s HTTP/1.1\r\n"), buff);
    bp.print(F("Host: "));
    bp.println(symoAddress);
    bp.println(F("Connection: close"));
    bp.printf(F("Content-Length: %d\r\n"), strlen_P(data));
    bp.printf(F("Authorization: Digest username=\"service\",realm=\"W\",nonce=\"ffcb243c6f951a5bab771e9d3b8f81d6\","
        "uri=\"%s\",response=\"" SUSCALIB_DIGEST_RESPONSE "\"\r\n"), buff);
    bp.println();
    strcpy_P(buff, data);
    bp.print(buff);
    bp.flush();

    client.setTimeout(4000);
    client.readBytesUntil(' ', buff, sizeof(buff)); // HTTP/1.1
    client.readBytesUntil(' ', buff, sizeof(buff)); // status code
    st = atoi(buff);
    while (client.read() != -1);
    client.stop();
  }
  eventsWrite(SUSPEND_CALIBRATION_EVENT, st, 0);
  done = true;
}
