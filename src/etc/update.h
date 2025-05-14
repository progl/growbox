
void configureHttpClientForFirefox(HTTPClient &http)
{
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:85.0) Gecko/20100101 Firefox/85.0");
    http.addHeader("Accept",
                   "text/html,application/xhtml+xml,application/"
                   "xml;q=0.9,image/webp,*/*;q=0.8");
    http.addHeader("Accept-Language", "en-US,en;q=0.5");
    http.addHeader("Accept-Encoding", "gzip, deflate, br");

    http.setConnectTimeout(100000);
    http.setTimeout(100000);
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

// Функция проверки свободного места
bool checkFreeSpace(size_t requiredSpace)
{
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeSpace = totalBytes - usedBytes;

    syslog_ng("update: Storage: Total: " + String(totalBytes) + ", Used: " + String(usedBytes) +
              ", Free: " + String(freeSpace));

    return freeSpace >= requiredSpace;
}

void updateFirmware(uint8_t *data, size_t len)
{
    Update.write(data, len);
    currentLength += len;
    Serial.print('.');

    if (currentLength != totalLength) return;
    syslog_ng("update: end update, Rebooting");
    boolean update_status = Update.end(true);
    if (Update.isFinished())
    {
        syslog_ng("update: update_status " + String(update_status));
        syslog_ng("update: update_status successfully Rebooting");
        preferences.putString(pref_reset_reason, "update");
        WiFi.disconnect(true);
        delay(100);
        esp_restart();
    }
    else
    {
        syslog_ng("update: update_status " + String(update_status));
        syslog_ng("update: Update not finished? Something went wrong!");
    }
}

bool makeHttpRequest(String url)

{
    syslog_ng("update: makeHttpRequest start");
    int retryCount = 0;
    const int maxRetries = 50;

    syslog_ng("update: makeHttpRequest ");
    int resp = 0;
    int http_begin = 0;

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    while (retryCount < maxRetries)
    {
        server.handleClient();

        if (!resp or resp == -1)
        {
            http_begin = http.begin(url, ca_cert);
            configureHttpClientForFirefox(http);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            resp = http.GET();
        }

        syslog_ng("update: Response " + String(resp) + " Retry " + String(retryCount) + " http_begin " +
                  String(http_begin) + " resp " + String(resp));
        if (resp == 200)
        {
            return true;  // Successful connection
        }
        else
        {
            syslog_ng("update:HTTP GET failed: resp " + String(resp) + " error " + http.errorToString(resp));
        }
        retryCount++;
        http.end();
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Delay between attempts
    }

    syslog_ng("update: Failed to connect after " + String(maxRetries) + " retries");
    return false;  // Connection failed
}
bool downloadAndUpdateIndexFile()
{
    String indexUrl = "https://ponics.online/static/wegabox/esp32-local/index.html.gz";

    int retryCount = 0;
    const int maxRetries = 50;

    int resp = 0;
    int http_begin = 0;

    while (retryCount < maxRetries)
    {
        server.handleClient();

        http_begin = http.begin(indexUrl, ca_cert);
        configureHttpClientForFirefox(http);
        syslog_ng("update: http_begin " + String(http_begin));

        vTaskDelay(50 / portTICK_PERIOD_MS);

        if (http_begin)
        {
            resp = http.GET();
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        syslog_ng("update:  index: Response " + String(resp) + " Retry " + String(retryCount));
        if (resp == 200)
        {
            break;
        }
        retryCount++;
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Задержка между попытками
    }

    if (resp != 200)
    {
        syslog_ng("update: Failed to download index.html.gz: " + String(resp));
        http.end();  // Закрываем предыдущее соединение
        return false;
    }

    File file = LittleFS.open("/index.html.gz", "w");
    if (!file)
    {
        syslog_ng("update: Failed to open index.html.gz for writing");
        http.end();  // Закрываем предыдущее соединение
        return false;
    }

    // Получаем поток данных
    WiFiClient *stream = http.getStreamPtr();

    int written = 0;
    unsigned long timeout = millis();

    // Читаем данные и записываем в файл
    while (http.connected())
    {
        // Проверка таймаута
        if (millis() - timeout > 100000)
        {  // 100 секунд таймаут
            syslog_ng("update: Download timeout");
            file.close();
            return false;
        }

        size_t size = stream->available();
        if (size)
        {
            timeout = millis();  // Сброс таймаута при получении данных
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            // Проверка успешности записи
            size_t bytesWritten = file.write(buff, c);
            if (bytesWritten != c)
            {
                syslog_ng("update: Write error: expected " + String(c) + " bytes, wrote " + String(bytesWritten));
                file.close();
                return false;
            }

            written += c;

            // Периодически освобождаем процессор
            if (written % 4096 == 0)
            {
                vTaskDelay(1);
            }
        }
        vTaskDelay(1);
    }

    file.close();
    http.end();  // Закрываем предыдущее соединение
    // Проверяем, что записали ожидаемое количество байт

    syslog_ng("update: Successfully updated index.html.gz, written " + String(written) + " bytes");
    return true;
}

void make_update()
{
    if ((server_make_update || force_update) && UPDATE_URL && !making_update)
    {
        making_update = true;
        syslog_ng("update: Starting firmware update...");
        syslog_ng("update: UPDATE_URL '" + String(UPDATE_URL) + "'");  // Log the update URL
        OtaStart = true;

        if (!downloadAndUpdateIndexFile())
        {
            syslog_ng("update: Failed to update index.html.gz");
        }
        syslog_ng("update: downloadAndUpdateIndexFile done");
        // Затем обновляем прошивку
        if (!makeHttpRequest(UPDATE_URL))
        {
            OtaStart = force_update = server_make_update = making_update = false;
            syslog_ng("update: HTTP request failed.");
            return;
        }
        totalLength = http.getSize();
        syslog_ng("update: Starting firmware update... UPDATE_URL " + String(UPDATE_URL) + " totalLength " +
                  String(totalLength));

        if (totalLength <= 0)
        {
            syslog_ng("update: Invalid firmware size");
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }
        bool upd = Update.begin(totalLength, U_FLASH);
        syslog_ng("update: begin " + String(upd));

        WiFiClient *stream = http.getStreamPtr();
        int len = totalLength;
        int bytesDownloaded = 0;
        int lastPercentageSent = -1;
        unsigned long updateStartTime = millis();
        unsigned long lastProgressTime = millis();

        while (http.connected() && (len > 0 || len == -1))
        {
            // Check total timeout
            if (millis() - updateStartTime > 900000)  // 90 seconds
            {
                syslog_ng("update: Total timeout exceeded");
                Update.abort();
                OtaStart = force_update = server_make_update = making_update = false;
                return;
            }

            // Check progress timeout
            if (millis() - lastProgressTime > 10000)  // 10 seconds without progress
            {
                syslog_ng("update: Progress timeout");
                Update.abort();
                OtaStart = force_update = server_make_update = making_update = false;
                return;
            }

            server.handleClient();
            size_t size = stream->available();
            if (size)
            {
                lastProgressTime = millis();  // Reset progress timeout
                int c = stream->readBytes(buff, min(size, sizeof(buff)));

                if (c <= 0)  // Check if reading was successful
                {
                    syslog_ng("update: Read failed or no data");
                    Update.abort();
                    OtaStart = force_update = server_make_update = making_update = false;
                    return;
                }

                if (Update.write(buff, c) != c)
                {
                    syslog_ng("update: Write failed c " + String(c));
                    Update.abort();
                    OtaStart = force_update = server_make_update = making_update = false;
                    return;
                }

                bytesDownloaded += c;
                if (len > 0)
                {
                    len -= c;
                }

                percentage = (bytesDownloaded * 100) / totalLength;
                if (percentage != lastPercentageSent)
                {
                    syslog_ng("update: Updating: " + String(percentage) + "%");
                    lastPercentageSent = percentage;
                }

                // Periodically yield to allow other tasks
                if (bytesDownloaded % 4096 == 0)
                {
                    vTaskDelay(1);
                }
            }
            vTaskDelay(1);
        }
        if (len != 0)
        {
            syslog_ng("update: Size mismatch");
            Update.abort();
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        if (!Update.end(true))
        {
            syslog_ng("update: Update end failed");
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        syslog_ng("update: Update successful, rebooting...");
        preferences.putString(pref_reset_reason, "update");
        delay(100);
        ESP.restart();
    }
    else
    {
        syslog_ng("update: auto-update skipping update due to conditions not met. force_update " +
                  String(force_update) + " server_make_update " + String(server_make_update) + " !making_update " +
                  String(!making_update) + " OtaStart " + String(OtaStart) + " percentage " + String(percentage) +
                  " UPDATE_URL " + String(UPDATE_URL));

        making_update = false;
        OtaStart = false;
        percentage = 0;
    }
}

void TaskUP(void *parameters) { make_update(); }

void update()
{
    if (server.arg("test").toInt() == 1)
    {
        syslog_ng("update: test - make_update");
        OtaStart = true;
        force_update = true;
        percentage = 1;
        make_update_send_response();
        xTaskCreate(TaskUP, "TaskUP", 10000, NULL, 0, NULL);
    }
    else
    {
        syslog_ng(
            "update:  make_update start update firmware - wait 40-60 seconds, page will "
            "reload automatically ");

        preferences.putInt("upd", 1);
        make_update_send_response();
        vTaskDelay(10);
        ESP.restart();
    }
}
