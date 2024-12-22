void configureHttpClientForFirefox(HTTPClient &http)
{
    // Это пример заголовка User-Agent для Firefox, версия и платформа могут отличаться
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:85.0) Gecko/20100101 Firefox/85.0");

    // Можно добавить и другие заголовки, которые обычно использует браузер
    http.addHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
    http.addHeader("Accept-Language", "en-US,en;q=0.5");
    http.addHeader("Accept-Encoding", "gzip, deflate, br");
    // Прочие заголовки по необходимости...
}

const char UPDATE_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<meta http-equiv="refresh" content="40;url=/">
start update firmware - wait 40-62 seconds, page will reload automatically
)=====";

void updatePage() { server.send_P(200, PSTR("text/html"), UPDATE_page); }

const char ERROR_page[] PROGMEM = R"=====(
<!DOCTYPE html>
 
ERROR
)=====";

void errorPage() { server.send_P(400, PSTR("text/html"), ERROR_page); }

void updateFirmware(uint8_t *data, size_t len)
{
    Update.write(data, len);
    currentLength += len;
    Serial.print('.');

    if (currentLength != totalLength) return;
    syslog_ng("end update, Rebooting");
    boolean update_status = Update.end(true);
    if (Update.isFinished())
    {
        syslog_ng("make_update: update_status " + String(update_status));
        syslog_ng("make_update: update_status successfully Rebooting");
        preferences.putString(pref_reset_reason, "update");
        WiFi.disconnect(true);
        delay(100);
        esp_restart();
    }
    else
    {
        syslog_ng("make_update: update_status " + String(update_status));
        syslog_ng("make_update: Update not finished? Something went wrong!");
    }
}

void update_f()
{
    if ((server_make_update || force_update) && UPDATE_URL && !making_update)
    {
        syslog_ng("make_update: starting ");
        client.stop();
        client.setInsecure();

        // Ограничение на количество попыток подключения
        const int maxRetries = 5;
        int retryCount = 0;

        while (retryCount < maxRetries)
        {
            con_c = client.connect("ponics.online", 443, 10000);
            if (con_c)
            {
                syslog_ng("make_update: Successfully connected to server.");
                break;
            }
            else
            {
                syslog_ng("make_update: Failed to connect, retrying... (" + String(retryCount + 1) + "/" +
                          String(maxRetries) + ")");
                retryCount++;
                vTaskDelay(1000 / portTICK_PERIOD_MS);  // Задержка между попытками
            }
        }

        // Если не удалось подключиться после всех попыток
        if (!con_c)
        {
            syslog_err("make_update: Could not connect to server after " + String(maxRetries) + " attempts.");
            return;
        }

        syslog_ng("make_update: UPDATE_URL " + UPDATE_URL);
        force_update = false;
        syslog_ng("make_update: Start TaskUpdate");
        OtaStart = true;
        String url = UPDATE_URL + "?token=" + update_token;
        syslog_ng("make_update: UPDATE_URL " + String(url));

        syslog_ng("make_update: con_c " + String(con_c));
        http.end();
        http.begin(client, url);
        configureHttpClientForFirefox(http);
        delay(5);
        server.handleClient();
        int resp = http.GET();
        delay(5);
        server.handleClient();

        syslog_ng("make_update: Response " + String(resp));
        int lastPercentageSent = -1;
        if (resp == 200 && !making_update)
        {
            making_update = true;
            syslog_ng("make_update starting update");
            OtaStart = true;
            totalLength = http.getSize();
            int len = totalLength;
            Update.begin(UPDATE_SIZE_UNKNOWN);
            syslog_ng("make_update: FW Size " + String(totalLength));
            uint8_t buff[512] = {0};
            WiFiClient *stream = http.getStreamPtr();
            syslog_ng("make_update: Updating firmware ");
            updatePage();

            int bytesDownloaded = 0;
            while (http.connected() && (len > 0 || len == -1))
            {
                server.handleClient();
                size_t size = stream->available();
                if (size)
                {
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                    updateFirmware(buff, c);
                    bytesDownloaded += c;
                    if (len > 0)
                    {
                        len -= c;
                    }
                    percentage = (bytesDownloaded * 100) / totalLength;
                    if (percentage == 0)
                    {
                        percentage = 1;
                    }
                    if (percentage != lastPercentageSent)
                    {
                        syslog_ng("make_update: Updating: " + String(percentage) + "%");
                        lastPercentageSent = percentage;
                    }
                    vTaskDelay(1);
                }
            }
            making_update = false;
            http.end();
            OtaStart = false;
        }
        else
        {
            making_update = false;
            OtaStart = false;
            percentage = 0;
            syslog_ng("make_update: error response code ");
            syslog_ng("make_update:END TaskUpdate");
        }
    }
}

void update()
{
    UPDATE_URL = server.arg("update_url");
    syslog_ng("make_update start update firmware - wait 40-60 seconds, page will reload automatically ");
    syslog_ng("make_update UPDATE_URL " + String(UPDATE_URL) + "?token=" + update_token);
    update_f();
}

void TaskUP(void *parameters) { update_f(); }