/*
void wemoLoop() {
  if (state != RegulatorState::REGULATING)
    return;
  measuredPower = wemoPowerUsage();
}
*/
int wemoSwitch(bool on) {
  char response[128];
  int res = wemoRequest("basicevent:1", "SetBinaryState", "BinaryState", on ? "1" : "0", response, sizeof(response));
  if (res < 0)
    return res;
  return (response[0] - '0');
}

int wemoPowerUsage() {
  char response[128];
  int res = wemoRequest("insight:1", "GetInsightParams", "InsightParams", nullptr, response, sizeof(response));
  if (res < 0) {
    msg.printf("W err %d", res);
    return -1;
  }
  char *p = response;
  for (int i = 0; i < 7; i++) {
    p = strchr(p, '|');
    if (p == NULL)
      return -2;
    p++;
  }
  return atol(p) / 1000;
}

int wemoRequest(const char* service, const char* action, const char* param, const char* value, char* response, size_t size) {

  const IPAddress wemoAddress(192,168,1,100);
  const int wemoPort = 49153;

  NetClient client;
  if (!client.connect(wemoAddress, wemoPort)) 
    return -1;

  char url[64] = "/upnp/control/";
  for (int i = 0, j = strlen(url); ; i++) {
    if (service[i] != ':') {
      url[j++] = service[i];
    }
    if (service[i] == 0)
      break;
  }

  char printBuff[64];
  ChunkedPrint chunked(client, printBuff, sizeof(printBuff));
  chunked.printf(F("POST %s HTTP/1.1\r\n"), url);
  chunked.print(F("Host: "));
  chunked.println(wemoAddress);
  chunked.println(F("Transfer-Encoding: chunked"));
  chunked.println(F("Connection: close"));
  chunked.printf(F("SOAPACTION: \"urn:Belkin:service:%s#%s\"\r\n"), service, action);
  chunked.println(F("Content-Type: text/xml"));
  chunked.println();
  chunked.begin();
  char sbBuff[64];
  CStringBuilder sb(sbBuff, sizeof(sbBuff));
  if (value != nullptr) {
    sb.printf(F("<%s>%s</%s>"), param, value, param);
  }
  chunked.printf(F("<?xml version='1.0' encoding='utf-8'?>"
                "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/' s:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/'>"
                    "<s:Body>"
                      "<u:%s xmlns:u='urn:Belkin:service:%s'>"
                         "%s"
                      "</u:%s>"
                    "</s:Body>"
                "</s:Envelope>"),
      action, service, sbBuff, action);
  chunked.end();
  sb.reset();
  sb.printf(F("<%s>"), param);
  int l = -2;
  client.setTimeout(1000);
  if (client.find(sbBuff)) {
    l = client.readBytesUntil('<', response, size - 1);
    if (l >= 0) {
      response[l] = 0;
    }
  }
  client.setTimeout(10);
  while (client.readBytes(sbBuff, sizeof(sbBuff)) != 0); // read the rest
  client.stop();
  return l;
}
