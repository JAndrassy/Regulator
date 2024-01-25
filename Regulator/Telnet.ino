
NetServer telnetServer(2323);

void telnetSetup() {
  telnetServer.begin();
}

void telnetLoop(boolean log) {

  static NetClient telnetClient;

  char buff[100];
  if (log) {
    unsigned long t = now();
    snprintf_P(buff, sizeof(buff), PSTR("%02d:%02d:%02d;%d;%c%d%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;"), hour(t), minute(t), second(t),
        freeMem, (char) state, mainRelayOn, bypassRelayOn,
        heatingPower, meterPower, pvSOC, pvChargingPower,
        powerPilotRaw, elsens, elsensPower, inverterAC, voltage, measuredPower);
#ifdef SERIAL_DEBUG
    SERIAL_DEBUG.print(buff);
    SERIAL_DEBUG.println(msgBuff);
#endif
  }
  if (!telnetClient.connected()) {
    telnetClient = telnetServer.accept();
    if (telnetClient.connected()) {
      telnetClient.println(F("Regulator\r\n"));
    }
  }
  if (telnetClient) {
    if (telnetClient.connected()) {
      if (log) {
        telnetClient.print(buff);
        telnetClient.println(msgBuff);
      }
      while (telnetClient.available()) {
        int c = telnetClient.read();
        switch (c) {
          case 'V':
            telnetClient.println(version);
            break;
          case 'C':
            while (telnetClient.read() != -1);
            telnetClient.stop();
          break;
          case 'E':
            eventsPrint(telnetClient);
          break;
          case 'R':
            telnetClient.stop();
            shutdown();
            while (true); // Watchdog test / reset
          break;
          case 'S': {
            BufferedPrint bp(telnetClient, buff, sizeof(buff));
            statsPrint(bp);
            bp.flush();
          }
          break;
        }
      }
      telnetClient.flush();
    } else {
      telnetClient.stop();
    }
  }
}

