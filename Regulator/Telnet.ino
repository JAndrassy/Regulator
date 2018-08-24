
NetServer telnetServer(2323);

void telnetSetup() {
  telnetServer.begin();
}

void telnetLoop(boolean log) {

  static NetClient telnetClient = telnetServer.available();

  char buff[100];
  CStringBuilder sb(buff, sizeof(buff));
  if (log) {
    unsigned long t = now();
    sb.printf(F("%02d:%02d:%02d;%d;%c%d%d%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;"), hour(t), minute(t), second(t),
        freeMem, (char) state, mainRelayOn, bypassRelayOn, balboaRelayOn,
        heatingPower, meterPower, pvSOC, pvChargingPower, availablePower,
        pwm, elsens, elsensPower, inverterAC, voltage, wemoPower);
    Serial.print(buff);
    Serial.println(msgBuff);
  }
  if (!telnetClient) {
    telnetClient = telnetServer.available();
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
          case 'E': {
            BufferedPrint bp(telnetClient, buff, sizeof(buff));
            eventsPrint(bp);
            bp.flush();
          }
          break;
          case 'R':
            telnetClient.stop();
            shutdown();
            while(true); // Watchdog test / reset
          break;
          case 'B': {
            BufferedPrint bp(telnetClient, buff, sizeof(buff));
            battSettRead(bp);
            bp.flush();
          }
          break;
          case 'S':
            BufferedPrint bp(telnetClient, buff, sizeof(buff));
            statsPrint(bp);
            bp.flush();
          break;
        }
      }
    } else {
      telnetClient.stop();
    }
  }
}

