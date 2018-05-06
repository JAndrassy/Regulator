
const IPAddress wemoAddress(192,168,1,100);
const int wemoPort = 49153;

void wemoLoop() {
  wemoPowerUsage();
}

boolean wemoPowerUsage() {
  char response[128];
  int res = wemoRequest("insight:1", "GetInsightParams", "InsightParams", nullptr, response, sizeof(response));
  if (res < 0)
    return false;
  char *p = response;
  for (int i = 0; i < 7; i++) {
    p = strchr(p, '|');
    if (p == NULL)
      return false;
    p++;
  }
  wemoPower = atol(p) / 1000;
  return true;
}



int wemoRequest(const char* service, const char* action, const char* param, const char* value, char* response, size_t size) {
  NetClient client;
  if (!client.connect(wemoAddress, wemoPort))
    return -2;
  char sbBuff[64];
  CStringBuilder sb(sbBuff, sizeof(sbBuff));
  sb.print(F("/upnp/control/"));
  char* colon = strchr(service, ':');
  colon[0] = 0;
  sb.print(service);
  sb.print(colon + 1);
  colon[0] = ':';

  char printBuff[64];
  ChunkedPrint chunked(client, printBuff, sizeof(printBuff));
  chunked.printf(F("POST %s HTTP/1.1\r\n"), sbBuff);
  chunked.print(F("Host: "));
  chunked.println(wemoAddress);
  chunked.println(F("Transfer-Encoding: chunked"));
  chunked.println(F("Connection: close"));
  chunked.printf(F("SOAPACTION: \"urn:Belkin:service:%s#%s\"\r\n"), service, action);
  chunked.println(F("Content-Type: text/xml"));
  chunked.println();
  chunked.begin();
  sb.reset();
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
  if (client.find(sbBuff)) {
    l = client.readBytesUntil('<', response, size);
    if (l >= 0) {
      response[l] = 0;
    }
  }
  while (client.read() != -1);
  client.stop();
  return l;
}
