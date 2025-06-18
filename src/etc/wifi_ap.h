void configAP()
{
    // Switch to AP only mode for better stability
    WiFi.mode(WIFI_AP);
    Serial.println("Switching to AP mode...");

    // Configure WiFi settings
    WiFi.setSleep(false);                 // Disable sleep mode for better performance
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Maximum transmit power

    // Configure AP
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(204); });

    server.on(
        "/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(200, "text/html", "<html><head><title>Success</title></head><body>Success</body></html>"); });

    // Handle captive portal requests
    server.on("/connecttest.txt", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "success"); });

    server.on("/checkip.php", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", "<html><body>Success</body></html>"); });

    server.on("/ncsi.txt", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "Microsoft NCSI"); });

    server.on(
        "/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(200, "text/html", "<html><head><title>Success</title></head><body>Success</body></html>"); });

    server.on(
        "/fwlink", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(200, "text/html", "<html><head><title>Success</title></head><body>Success</body></html>"); });

    // Handle all other requests

    // Set static IP configuration
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAPsetHostname("growbox");

    // Start DNS server
    DNSServer dnsServer;
    dnsServer.setTTL(60);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", local_ip);
    // Parameters: SSID, password, channel, hidden, max_connection
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str(), 11, 0, 4);
    isAPMode = true;
    IPAddress ip = WiFi.softAPIP();
    Serial.print("Внимание!!!! Вайфай режим точки доступа. SSID: ");
    Serial.print(AP_SSID);
    Serial.print(" Пароль ");
    Serial.print(AP_PASS);
    Serial.print(" Веб логин/пароль admin/ponics ");
    Serial.print(", enter http://");

    Serial.println(ip);
}

bool tryConnectToWiFi()
{
    // Переключаемся в режим STA (клиент)
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.print("Попытка подключения к WiFi: ");
    Serial.print(" пароль ");
    Serial.print(password);
    Serial.print(" ssid ");
    Serial.println(ssid);
    vTaskDelay(pdTICKS_TO_MS(2000));

    // Ожидаем подключения с таймаутом
    unsigned long startAttemptTime = millis();
    while (millis() - startAttemptTime < 20000)
    {  // 5 секунд таймаут
        if (WiFi.status() == WL_CONNECTED)
        {
            wifiIp = WiFi.localIP().toString();
            syslog_ng("Успешное подключение к WiFi. IP адрес: " + wifiIp);
            return true;
        }
        vTaskDelay(pdTICKS_TO_MS(1000));
        Serial.print("Попытка подключения к WiFi " + String(millis()));
    }

    // Если не удалось подключиться
    Serial.println("Не удалось подключиться к WiFi");

    return false;
}

bool connectToWiFi()
{
    // Сначала пытаемся подключиться к WiFi
    if (tryConnectToWiFi())
    {
        // Если подключились успешно и есть активная точка доступа - отключаем её
        if (WiFi.getMode() & WIFI_AP)
        {
            Serial.println("Отключаем точку доступа, так как подключились к WiFi");
            WiFi.softAPdisconnect(true);
        }
        return true;  // Успешное подключение к WiFi
    }

    // Если не удалось подключиться к WiFi - включаем точку доступа
    if (!(WiFi.getMode() & WIFI_AP))
    {
        Serial.println("Переход в режим точки доступа");
        configAP();
    }
    return false;  // Не удалось подключиться к WiFi
}

void checkWiFiConnection(TimerHandle_t xTimer)
{
    static unsigned long lastCheckTime = 0;
    const unsigned long checkInterval = 60000 * 5;  // Интервал проверки 5м

    // Не проверяем слишком часто, если уже подключены
    if (millis() - lastCheckTime < checkInterval && WiFi.status() == WL_CONNECTED)
    {
        return;
    }
    lastCheckTime = millis();

    // Если не подключены к WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Нет подключения к WiFi. Пытаемся переподключиться...");

        // Если в режиме AP, не пытаемся подключаться - остаёмся в режиме точки доступа
        if (WiFi.getMode() & WIFI_AP)
        {
            return;
        }

        // Пытаемся подключиться к WiFi
        connectToWiFi();
    }
    else
    {
        // Если подключены к WiFi и при этом активна точка доступа - отключаем её
        if (WiFi.getMode() & WIFI_AP)
        {
            Serial.println("Подключились к WiFi, отключаем точку доступа...");
            WiFi.softAPdisconnect(true);
        }
    }
}