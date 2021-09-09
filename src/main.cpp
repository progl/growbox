
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
WebServer server(80);

#include <WiFiClient.h>
#include <HTTPClient.h>
#include <pre.h>

#include <func>

// Переменные
float AirTemp, AirHum, RootTemp, CO2, tVOC,seta;


#define HOSTNAME "WEGABOX" // Имя системы и DDNS .local
#define ONE_WIRE_BUS 23    // Порт 1-Wire
#include <Wire.h>          // Шина I2C
#define I2C_SDA 21         // SDA
#define I2C_SCL 22         // SCL
 
#define DS18B20 1 
#define c_AHT10 1
#define c_AM2320 0
#define c_CCS811 1


#if DS18B20 == 1
  #include <OneWire.h>
  #include <DallasTemperature.h>
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature sens18b20(&oneWire);
#endif

#if c_AHT10 == 1
  #include <AHT10.h>
  AHT10 myAHT10(AHT10_ADDRESS_0X38);
#endif

#if c_CCS811 == 1
  #include "ccs811.h"  // CCS811 library
  CCS811 ccs811;
#endif


void handleRoot() {
  String httpstr="<meta http-equiv='refresh' content='10'>";
       httpstr += "HOSTNAME=" + String(HOSTNAME) + "<br>";
       
       if(RootTemp) { httpstr +=  "RootTemp=" + fFTS(RootTemp,3) + "<br>"; }
       if(AirTemp)  { httpstr +=  "AirTemp=" +  fFTS(AirTemp,3) + "<br>"; }
       if(AirHum)   { httpstr +=  "AirHum=" +   fFTS(AirHum,3) + "<br>"; }
       if(CO2)   { httpstr +=  "CO2=" +   fFTS(CO2,3) + "<br>"; }
       if(tVOC)   { httpstr +=  "tVOC=" +   fFTS(tVOC,3) + "<br>"; }
        httpstr +=  "seta=" +   fFTS(seta,3) + "<br>";      
       

  server.send(200, "text/html",  httpstr);
  }


void Task1(void * parameters){
  for(;;){
    seta=13;
    vTaskDelay(1000/ portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname( HOSTNAME );
  ArduinoOTA.begin();
  MDNS.begin( HOSTNAME );
  MDNS.addService("http", "tcp", 80);
  server.on("/",handleRoot);
  server.begin();

  #if DS18B20 == 1
  sens18b20.begin();
  sens18b20.setResolution(12);
  #endif

  #if c_AHT10 == 1
  Wire.begin(I2C_SDA, I2C_SCL);
  myAHT10.begin();
  #endif

  #if c_CCS811 == 1
    // Enable CCS811
  ccs811.set_i2cdelay(50); // Needed for ESP8266 because it doesn't handle I2C clock stretch correctly
  ccs811.begin();
  ccs811.start(CCS811_MODE_1SEC);
  #endif


  xTaskCreate(Task1,"Task1",1000,NULL,0,NULL);

  server.handleClient();
  ArduinoOTA.handle();

}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  #if DS18B20 == 1
  sens18b20.requestTemperatures();
  RootTemp=sens18b20.getTempCByIndex(0);
  #endif
 
  #if c_AHT10 == 1
  AirTemp=myAHT10.readTemperature();
  AirHum=myAHT10.readHumidity();
  #endif

  #if c_CCS811 == 1
      // Read
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2,&etvoc,&errstat,&raw); 
    
    // Print measurement results based on status
    if( errstat==CCS811_ERRSTAT_OK ) { 
      CO2=eco2;
      tVOC=etvoc;
    } 
  #endif




}






