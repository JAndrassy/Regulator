
#ifdef ETHERNET
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
#endif

#define MY_BLYNK_TOKEN "YourBlynkToken"
#define SUSCALIB_DIGEST_RESPONSE "DigestAuthResponse"

//ha1 = md5("service:W:PASSWORD") // replace PASSWORD
//ha2 = md5("POST:/servicecgi-bin/suspend_battery_calibration/?method=save")
//response = md5(ha1 + ":ffcb243c6f951a5bab771e9d3b8f81d6:" + ha2)

