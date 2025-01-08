uint8_t buff[512] = {0};
void configureHttpClientForFirefox(HTTPClient &http)
{
    // Это пример заголовка User-Agent для Firefox, версия и платформа могут
    // отличаться
    http.addHeader("User-Agent",
                   "Mozilla/5.0 (Windows NT 10.0; Win64; x64; "
                   "rv:85.0) Gecko/20100101 Firefox/85.0");

    // Можно добавить и другие заголовки, которые обычно использует браузер
    http.addHeader("Accept",
                   "text/html,application/xhtml+xml,application/"
                   "xml;q=0.9,image/webp,*/*;q=0.8");
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

bool makeHttpRequest(String url)
{
    int retryCount = 0;
    const int maxRetries = 50;

    while (retryCount < maxRetries)
    {
        server.handleClient();
        http.end();  // Закрываем предыдущее соединение
        vTaskDelay(50 / portTICK_PERIOD_MS);
        http.begin(client, url);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        int resp = http.GET();

        syslog_ng("make_update: Response " + String(resp) + " Retry " + String(retryCount));
        if (resp == 200)
        {
            return true;  // Успешное соединение
        }
        retryCount++;
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Задержка между попытками
    }

    syslog_ng("make_update: Failed to connect after " + String(maxRetries) + " retries");
    return false;  // Соединение не удалось
}

void update_f()
{
    if ((server_make_update || force_update) && UPDATE_URL && !making_update)
    {
        syslog_ng("make_update: Starting...");
        OtaStart = true;
        client.stop();
        http.end();
        delay(5);
        client.setInsecure();
        making_update = true;
        if (!makeHttpRequest(UPDATE_URL))
        {
            OtaStart = force_update = server_make_update = making_update = false;
            syslog_ng("make_update: HTTP request failed.");
            return;
        }
        syslog_ng("make_update: Starting firmware update...");

        totalLength = http.getSize();
        Update.begin(UPDATE_SIZE_UNKNOWN);
        syslog_ng("make_update: FW Size " + String(totalLength));

        WiFiClient *stream = http.getStreamPtr();
        int len = totalLength;
        int bytesDownloaded = 0;
        int lastPercentageSent = -1;

        // Засекаем время начала обновления
        unsigned long updateStartTime = millis();

        while (http.connected() && (len > 0 || len == -1))
        {
            // Проверяем, не превысило ли время обновления лимит (1 минута)
            if (millis() - updateStartTime > 90000)
            {
                syslog_ng("make_update: Timeout exceeded, aborting update");
                http.end();
                return;  // Прерываем процесс обновления
            }

            server.handleClient();
            size_t size = stream->available();
            if (size)
            {
                int c = stream->readBytes(buff, min(size, sizeof(buff)));
                updateFirmware(buff, c);
                bytesDownloaded += c;

                if (len > 0)
                {
                    len -= c;
                }

                percentage = (bytesDownloaded * 100) / totalLength;
                if (percentage != lastPercentageSent)
                {
                    syslog_ng("make_update: Updating: " + String(percentage) + "%");
                    lastPercentageSent = percentage;
                }
                vTaskDelay(1);
            }
        }

        http.end();
        making_update = false;
        OtaStart = false;

        if (bytesDownloaded == totalLength)
        {
            syslog_ng("make_update: Update completed successfully.");
        }
        else
        {
            syslog_ng("make_update: Update incomplete. Downloaded " + String(bytesDownloaded) + "/" +
                      String(totalLength));
        }
    }
    else
    {
        syslog_ng("make_update: Skipping update due to conditions not met.");
        http.end();
        making_update = false;
        OtaStart = false;
        percentage = 0;
    }
}

void update()
{
    UPDATE_URL = server.arg("update_url");
    syslog_ng(
        "make_update start update firmware - wait 40-60 seconds, page will "
        "reload automatically ");
    syslog_ng("make_update UPDATE_URL " + String(UPDATE_URL) + "?token=" + update_token);
    update_f();
}

void TaskUP(void *parameters) { update_f(); }