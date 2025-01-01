#define DISABLE_TLS
#define SYSLOG_SERVER ""  // Адрес SYSLOG сервера
#define SYSLOG_PORT 514   // Порт SYSLOG сервера
String HOSTNAME = "growbox";
String AP_SSID = "ESP32WEGA56";
String AP_PASS = "ESP32WEGA56";

String update_token = "";
String wegadb = "";

String ssid = AP_SSID;
String password = AP_PASS;

IPAddress local_ip(192, 168, 50, 1);
IPAddress gateway(192, 168, 50, 1);
IPAddress dhcpstart(192, 168, 50, 10);
IPAddress subnet(255, 255, 255, 0);

String appName;
String wifiIp;
String UPDATE_URL = "https://ponics.online/static/wegabox/esp32-local/firmware.bin";

int US025_CHANGE_PORTS = 0;
int ENABLE_AUTO_UPDATES = 1;
int COMPILE_ID = 0;
int put_local_adress_toServer = 1;

#include <fw_commit.h>
String Firmware = firmware_commit;
boolean force_update = false;
int percentage = 0;
IPAddress serverIP;
String a_ha, p_ha, u_ha;

long PumpB_Step_SUM, PumpA_Step_SUM;
float SetPumpA_Ml_SUM, SetPumpB_Ml_SUM;
int clear_pref;
int DRV1_A = 0;
int DRV1_B = 1;
int DRV1_C = 2;
int DRV1_D = 3;
int DRV2_A = 4;
int DRV2_B = 5;
int DRV2_C = 6;
int DRV2_D = 7;
int DRV3_A = 8;
int DRV3_B = 9;
int DRV3_C = 10;
int DRV3_D = 11;
int DRV4_A = 12;
int DRV4_B = 13;
int DRV4_C = 14;
int DRV4_D = 15;
int calibrate_now = 0;