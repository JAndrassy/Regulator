package rlk;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.URL;

public class RegulatorDev {

  static final String SYMO_ADDRESS = "192.168.1.7";
  static final String SWITCH_ADDRESS = "192.168.1.100:49153";
  static final String ARDUINO_ADDRESS = "192.168.1.8";
  static final int SWITCH_OFF = 0;
  static final int SWITCH_STANDBY = 8;
  static final int SWITCH_ON = 1;

  Socket arduinoTelnetSocket;
  Socket froniusModbusSocket;

  void delay(int ms) {
    try {
      Thread.sleep(ms);
    } catch (InterruptedException e) {
    }
  }
  
  int systemPowerUsage() {
    String res = upnpWemoRequest(SWITCH_ADDRESS, "insight:1", "GetInsightParams", "InsightParams", null);
    String[] ss = res.split("\\|");
    return Integer.parseInt(ss[7]) / 1000;
  }
  
  void setSwitch(int state) {
    upnpWemoRequest(SWITCH_ADDRESS, "basicevent:1", "SetBinaryState", "BinaryState", "" + state);
  }
  
  int[] froniusModbusRequest(int uid, int address, int len) {
    try {
      if (froniusModbusSocket == null || froniusModbusSocket.isClosed()) {
        froniusModbusSocket = new Socket(SYMO_ADDRESS, 502);
        froniusModbusSocket.setKeepAlive(true);
      }
      OutputStream os = froniusModbusSocket.getOutputStream();

      byte[] req = new byte[] { 0, 1, 0, 0, 0, 6, (byte) uid, 3, // header
          (byte) (address / 256), (byte) (address % 256), 0, (byte) len };

      os.write(req);
      os.flush();

      InputStream is = froniusModbusSocket.getInputStream();
      is.skip(7); // ids
      int code = is.read();
      if (code == 0x83)
        throw new RuntimeException("modbus error " + is.read());
      if (code != 3)
        throw new RuntimeException("modbus response error fnc = " + code);
      int l = is.read() / 2;
      int[] response = new int[len];
      int i = 0;
      byte[] buff = new byte[2];
      while (true) {
        if (is.read(buff) == -1)
          break;
        int hi = buff[0] & 0xFF;
        int lo = buff[1] & 0xFF;
        response[i++] = hi * 256 + lo;
        if (i == l)
          break;
      }
      return response;
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }
  
  int arduinoRequest(int value) {
    try {
      if (arduinoTelnetSocket == null || arduinoTelnetSocket.isClosed()) {
        arduinoTelnetSocket = new Socket();
        arduinoTelnetSocket.connect(new InetSocketAddress(ARDUINO_ADDRESS, 2323), 1000);
        arduinoTelnetSocket.setKeepAlive(true);
      }
      OutputStream os = arduinoTelnetSocket.getOutputStream();
      os.write((value + "\n").getBytes());
      os.flush();
      BufferedReader r = new BufferedReader(new InputStreamReader(arduinoTelnetSocket.getInputStream()));
      String s = r.readLine();
      return Integer.parseInt(s);
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }
  
	void arduinoClose() {
		try {
			arduinoTelnetSocket.close();
		} catch (IOException e) {
		}
	}
  
  String upnpWemoRequest(String host, String service, String action, String param, String value) {
    try {
      URL url = new URL("http://" + host + "/upnp/control/" + service.replaceAll(":", ""));
      HttpURLConnection con = (HttpURLConnection) url.openConnection();
      con.setDoOutput(true);
      con.setRequestMethod("POST");
      con.setRequestProperty("SOAPACTION", "\"urn:Belkin:service:" + service + "#" + action + "\"");
      con.setRequestProperty("Content-Type", "text/xml");
      OutputStream os = con.getOutputStream();
      os.write(new String(
          "<?xml version='1.0' encoding='utf-8'?>" +
                  "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/' s:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/'>" +
                      "<s:Body>" +
                        "<u:" + action + " xmlns:u='urn:Belkin:service:" + service +"'>" +
                          (value != null ? "<" + param + ">" + value + "</" + param + ">"  : "")+ 
                        "</u:" + action + ">" +
                      "</s:Body>" +
                  "</s:Envelope>").getBytes("UTF-8"));
      BufferedReader in = new BufferedReader(new InputStreamReader(con.getInputStream()));
      String output;
      StringBuffer response = new StringBuffer();
      while ((output = in.readLine()) != null) {
        response.append(output);
      }
      in.close();
      return response.substring(response.indexOf("<" + param + ">") + param.length() + 2, response.indexOf("</" + param + ">"));
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  public static void main(String[] args) throws Exception {
    RegulatorDev r = new RegulatorDev();
//    while (true) {
//      System.out.println(new Date(System.currentTimeMillis()).toString() + "\t" + (r.systemPowerUsage() - 50));
//      Thread.sleep(5000);
//    }
    try {
//      r.setSwitch(SWITCH_OFF);
//      r.delay(10000);
//      r.setSwitch(SWITCH_ON);
//      r.delay(10000);
////      r.arduinoRequest(255);
////      Thread.sleep(60000);
////      r.arduinoRequest(0);
////      Thread.sleep(30000);
//
      int baseEs = r.arduinoRequest(0);
      int[] response = r.froniusModbusRequest(241, 40087 , 5);
      int baseMeter = (int) ((short) response[0] * Math.pow(10, (short) response[4]));
//      int base = r.systemPowerUsage();
      System.out.println(baseEs + ", " + baseMeter);

      for (int p = 9000; p >= 0; p -= 500) {
        r.arduinoRequest(p);
        r.delay(1000);
        int elsens = r.arduinoRequest(p);
        response = r.froniusModbusRequest(241, 40087 , 5);
        int meter = (int) ((short) response[0] * Math.pow(10, (short) response[4]));
//        int wm = r.systemPowerUsage();
        System.out.println(p + "\t" + elsens + "\t" + meter);
      }
      r.arduinoRequest(0);
    } finally {
      r.arduinoClose();
//      r.setSwitch(SWITCH_OFF);
    }
  }
  
}
