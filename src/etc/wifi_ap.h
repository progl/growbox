

void configAP()
{
    WiFi.mode(WIFI_AP_STA);  // starts the default AP (factory default or setup as persistent)
    Serial.print("Connect your computer to the WiFi network ");
    Serial.print("to SSID of you ESP32");                 // no getter for SoftAP SSID
    server.on("/generate_204", handleRoot);               // Для Android
    server.on("/library/test/success.html", handleRoot);  // Для iOS
    WiFi.softAPConfig(local_ip, gateway, subnet, dhcpstart);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str(), 1, 0, 4, false);
    IPAddress ip = WiFi.softAPIP();
    Serial.print("and enter http://");
    Serial.print(ip);
    Serial.println(" in a Web browser");
}