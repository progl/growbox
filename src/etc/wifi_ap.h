

void configAP()
{
    WiFi.mode(WIFI_AP);  // starts the default AP (factory default or setup as
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

void setupWiFi()
{
    Serial.println("Start wifi config ssid " + String(ssid) + " password " + String(password));

    syslog.deviceHostname(HOSTNAME.c_str());
    syslog.appName(appName.c_str());
    WiFi.setHostname(HOSTNAME.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Error connecting to WiFi");
        configAP();
    }
    else
    {
        Serial.println("WIFI connected");
    }

    wifiIp = WiFi.localIP().toString();
    syslog_ng("wifiIp " + String(wifiIp) + " HOSTNAME " + String(WiFi.getHostname()));
    Serial.println("after wifi config");
    Serial.println(WiFi.localIP());
}
