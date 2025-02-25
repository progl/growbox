void handleRedirect()
{
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

void configAP()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    Serial.println("Switching to AP mode...");

    server.on("/generate_204", handleRoot);
    server.on("/library/test/success.html", handleRoot);
    WiFi.softAPConfig(local_ip, gateway, subnet, dhcpstart);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str(), 1, 0, 4, false);

    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP mode. Connect to SSID: ");
    Serial.print(AP_SSID);
    Serial.print(", enter http://");
    Serial.println(ip);

    isAPMode = true;
}

bool tryConnectToWiFi()
{
    Serial.print("Trying to connect to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
        Serial.println("Connected to WiFi.");
        wifiIp = WiFi.localIP().toString();
        Serial.print("IP Address: ");
        Serial.println(wifiIp);
        isAPMode = false;
        return true;
    }
    Serial.println("Failed to connect to WiFi.");
    return false;
}

void connectToWiFi()
{
    if (tryConnectToWiFi())
    {
        if (isAPMode)
        {
            Serial.println("Disabling AP mode after successful WiFi connection.");
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            isAPMode = false;
        }
    }
    else
    {
        if (!isAPMode)
        {
            configAP();
        }
    }
}

void checkWiFiConnection(TimerHandle_t xTimer)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected. Checking connection...");

        if (isAPMode)
        {
            Serial.println("In AP mode. Trying to reconnect to WiFi...");
            connectToWiFi();
        }
        else
        {
            Serial.println("Lost connection in STA mode. Switching to AP...");
            configAP();
        }
    }
}