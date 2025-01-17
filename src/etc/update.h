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

// Функция проверки свободного места
bool checkFreeSpace(size_t requiredSpace) {
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeSpace = totalBytes - usedBytes;
    
    syslog_ng("Storage: Total: " + String(totalBytes) + ", Used: " + String(usedBytes) + ", Free: " + String(freeSpace));
    
    return freeSpace >= requiredSpace;
}

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

bool downloadAndUpdateIndexFile()
{
    String indexUrl = "https://ponics.online/static/wegabox/esp32-local/index.html.gz";
    
    http.end();  // Закрываем предыдущее соединение
    http.begin(client, indexUrl);
    configureHttpClientForFirefox(http);
    
    // Получаем размер файла
    int resp = http.GET();
    if (resp != 200) {
        syslog_ng("Failed to download index.html.gz: " + String(resp));
        return false;
    }
    
    int fileSize = http.getSize();
    if (fileSize <= 0) {
        syslog_ng("Invalid file size: " + String(fileSize));
        return false;
    }

    // Проверяем свободное место
    if (!checkFreeSpace(fileSize + 1024)) { // +1KB для безопасности
        syslog_ng("Not enough free space for index.html.gz");
        return false;
    }

    // Открываем файл для записи
    File file = LittleFS.open("/index.html.gz", "w");
    if (!file) {
        syslog_ng("Failed to open index.html.gz for writing");
        return false;
    }

    // Получаем поток данных
    WiFiClient *stream = http.getStreamPtr();
    int len = fileSize;
    int written = 0;
    unsigned long timeout = millis();

    // Читаем данные и записываем в файл
    while (http.connected() && (len > 0 || len == -1)) {
        // Проверка таймаута
        if (millis() - timeout > 10000) { // 10 секунд таймаут
            syslog_ng("Download timeout");
            file.close();
            return false;
        }

        size_t size = stream->available();
        if (size) {
            timeout = millis(); // Сброс таймаута при получении данных
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            
            // Проверка успешности записи
            size_t bytesWritten = file.write(buff, c);
            if (bytesWritten != c) {
                syslog_ng("Write error: expected " + String(c) + " bytes, wrote " + String(bytesWritten));
                file.close();
                return false;
            }
            
            written += c;
            if (len > 0) {
                len -= c;
            }

            // Периодически освобождаем процессор
            if (written % 4096 == 0) {
                vTaskDelay(1);
            }
        }
        vTaskDelay(1);
    }

    file.close();

    // Проверяем, что записали ожидаемое количество байт
    if (written != fileSize) {
        syslog_ng("Size mismatch: expected " + String(fileSize) + ", got " + String(written));
        return false;
    }

    syslog_ng("Successfully updated index.html.gz, written " + String(written) + " bytes");
    return true;
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

        // Проверяем свободное место для прошивки
        size_t updateSize = UPDATE_SIZE_UNKNOWN; // Можно задать примерный размер
        if (!checkFreeSpace(updateSize)) {
            syslog_ng("Not enough free space for firmware update");
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        // Сначала обновляем index.html.gz
        if (!downloadAndUpdateIndexFile()) {
            syslog_ng("Failed to update index.html.gz");
        }

        // Затем обновляем прошивку
        if (!makeHttpRequest(UPDATE_URL))
        {
            OtaStart = force_update = server_make_update = making_update = false;
            syslog_ng("make_update: HTTP request failed.");
            return;
        }

        syslog_ng("make_update: Starting firmware update...");
        totalLength = http.getSize();
        
        if (totalLength <= 0) {
            syslog_ng("make_update: Invalid firmware size");
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        if (!Update.begin(totalLength)) {
            syslog_ng("make_update: Not enough space for update");
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        syslog_ng("make_update: FW Size " + String(totalLength));

        WiFiClient *stream = http.getStreamPtr();
        int len = totalLength;
        int bytesDownloaded = 0;
        int lastPercentageSent = -1;
        unsigned long updateStartTime = millis();
        unsigned long lastProgressTime = millis();

        while (http.connected() && (len > 0 || len == -1))
        {
            // Проверка общего таймаута
            if (millis() - updateStartTime > 90000) // 90 секунд
            {
                syslog_ng("make_update: Total timeout exceeded");
                Update.abort();
                OtaStart = force_update = server_make_update = making_update = false;
                return;
            }

            // Проверка таймаута прогресса
            if (millis() - lastProgressTime > 10000) // 10 секунд без прогресса
            {
                syslog_ng("make_update: Progress timeout");
                Update.abort();
                OtaStart = force_update = server_make_update = making_update = false;
                return;
            }

            server.handleClient();
            size_t size = stream->available();
            if (size)
            {
                lastProgressTime = millis(); // Сброс таймаута прогресса
                int c = stream->readBytes(buff, min(size, sizeof(buff)));
                
                if (Update.write(buff, c) != c) {
                    syslog_ng("make_update: Write failed");
                    Update.abort();
                    OtaStart = force_update = server_make_update = making_update = false;
                    return;
                }

                bytesDownloaded += c;
                if (len > 0) {
                    len -= c;
                }

                percentage = (bytesDownloaded * 100) / totalLength;
                if (percentage != lastPercentageSent)
                {
                    syslog_ng("make_update: Updating: " + String(percentage) + "%");
                    lastPercentageSent = percentage;
                }
                
                // Периодически освобождаем процессор
                if (bytesDownloaded % 4096 == 0) {
                    vTaskDelay(1);
                }
            }
            vTaskDelay(1);
        }

        if (len != 0) {
            syslog_ng("make_update: Size mismatch");
            Update.abort();
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        if (!Update.end(true)) {
            syslog_ng("make_update: Update end failed");
            OtaStart = force_update = server_make_update = making_update = false;
            return;
        }

        syslog_ng("make_update: Update successful, rebooting...");
        preferences.putString(pref_reset_reason, "update");
        delay(100);
        ESP.restart();
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