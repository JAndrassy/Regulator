
NetServer telnetServer(2323);

void telnetSetup() {
  telnetServer.begin();
}

void telnetLoop(boolean log) {

  static NetClient telnetClient = telnetServer.available();
  char tempBuff[100];

  if (log) {
    t2s(now(), tempBuff);
    sprintfF(tempBuff + 8, F(";%d;%c%d%d%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;"), freeMemory(), (char) state, mainRelayOn, bypassRelayOn, balboaRelayOn,
        heatingPower, m, soc, b, availablePower, pwm, elsens, elsensPower, inverterAC);
    Serial.print(tempBuff);
    Serial.println(msg);
  }
  if (!telnetClient) {
    telnetClient = telnetServer.available();
  }
  if (telnetClient) {
    if (telnetClient.connected()) {
      if (log) {
        telnetClient.print(tempBuff);
        telnetClient.println(msg);
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
            while(telnetClient.read() != 'R') {
              delay(100);
            }
          break;
          case 'B':
            battSettRead(telnetClient);
          break;
        }
      }
    } else {
      telnetClient.stop();
    }
  }
}

