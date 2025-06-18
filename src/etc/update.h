

bool createDirectories(String path)
{
    if (path.startsWith("/"))
    {
        path = path.substring(1);
    }

    String currentPath = "";
    int start = 0;
    int end = path.indexOf('/');

    while (end != -1)
    {
        currentPath += "/" + path.substring(start, end);
        if (!LittleFS.exists(currentPath))
        {
            if (!LittleFS.mkdir(currentPath))
            {
                return false;
            }
        }
        start = end + 1;
        end = path.indexOf('/', start);
    }

    return true;
}
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

void updateFirmware(uint8_t* data, size_t len)
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

// Add this helper function at the top of the file
void closeAllOpenFiles()
{
    // This is a workaround since there's no direct API to close all open files
    // We'll just open and close a file to force any pending operations
    File f = LittleFS.open("/.trash", "w");
    if (f)
    {
        f.close();
        LittleFS.remove("/.trash");
    }
    yield();  // Let the filesystem finish any pending operations
}

bool downloadAsset(const String& url, const String& localPath)
{
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    http.setReuse(true);
    http.setTimeout(20000);
    syslog_ng("downloadAsset: " + url);
    // Get file size first
    if (!http.begin(client, url.c_str()))
    {
        syslog_ng("downloadAsset: Failed to connect to " + url);
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        syslog_ng("HTTP GET failed, error: " + String(http.errorToString(httpCode).c_str()));
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        syslog_ng("downloadAsset: Invalid content length");
        http.end();
        return false;
    }

    // Remove existing file if it exists
    if (LittleFS.exists(localPath))
    {
        closeAllOpenFiles();
        LittleFS.remove(localPath);
        syslog_ng("remove old localPath " + localPath);
    }
    else if (LittleFS.exists("/assets/" + localPath))
    {
        closeAllOpenFiles();
        LittleFS.remove("/assets/" + localPath);
        syslog_ng("remove old /assets/" + localPath + " " + localPath);
    }
    // Check free space (add some extra margin)
    if (!checkFreeSpace(contentLength + 1024))
    {
        syslog_ng("Not enough free space to download " + url);
        http.end();
        return false;
    }

    // Create parent directories
    String dirPath = localPath.substring(0, localPath.lastIndexOf('/'));
    if (dirPath.length() > 0 && !createDirectories(dirPath))
    {
        syslog_ng("Failed to create directory: " + dirPath);
        http.end();
        return false;
    }

    // Open file for writing
    File file = LittleFS.open(localPath, "w");
    if (!file)
    {
        syslog_ng("Failed to open file for writing: " + localPath);
        http.end();
        return false;
    }

    // Get the stream
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[512];
    size_t totalRead = 0;

    while (http.connected() && totalRead < (size_t)contentLength)
    {
        size_t bytesAvailable = stream->available();
        if (bytesAvailable)
        {
            int c = stream->readBytes(buffer, min(sizeof(buffer), bytesAvailable));
            if (c > 0)
            {
                size_t written = file.write(buffer, c);
                if (written != (size_t)c)
                {
                    syslog_ng("Write failed, written: " + String(written) + " expected: " + String(c));
                    file.close();
                    LittleFS.remove(localPath);
                    http.end();
                    return false;
                }
                totalRead += written;
                yield();  // Let other tasks run
            }
        }
        delay(1);
    }

    file.close();
    http.end();

    if (totalRead != (size_t)contentLength)
    {
        syslog_ng("Download incomplete: " + String(totalRead) + " of " + String(contentLength));
        LittleFS.remove(localPath);
        return false;
    }

    syslog_ng("Successfully downloaded: " + localPath + " (" + String(totalRead) + " bytes)");
    return true;
}

void clearLittleFS()
{
    // Initialize LittleFS
    if (!LittleFS.begin(true))
    {
        Serial.println("Failed to mount LittleFS");
        return;
    }

    // Open root directory
    File root = LittleFS.open("/");
    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    // List all files and remove them
    File file = root.openNextFile();
    while (file)
    {
        String path = file.path();
        bool isDir = file.isDirectory();
        file.close();

        if (isDir)
        {
            // If you have directories, you might want to handle them recursively
            // For now, we'll just skip them
            Serial.printf("Skipping directory: %s\n", path.c_str());
        }
        else
        {
            if (LittleFS.remove(path))
            {
                Serial.printf("Removed file: %s\n", path.c_str());
            }
            else
            {
                Serial.printf("Failed to remove: %s\n", path.c_str());
            }
        }
        file = root.openNextFile();
    }

    Serial.println("Finished clearing LittleFS");
}

bool downloadAndUpdateIndexFile()
{
    const String baseUrl = "https://ponics.online/static/wegabox/esp32-local/";
    const String assetsManifestUrl = baseUrl + "assets/manifest.json";
    const int maxRetries = 3;

    // Initialize filesystem
    if (!LittleFS.begin(true))
    {
        syslog_ng("Failed to initialize LittleFS");
        return false;
    }
    clearLittleFS();
    // Download manifest
    String manifest;
    for (int attempt = 0; attempt < maxRetries; attempt++)
    {
        syslog_ng("Downloading manifest, attempt " + String(attempt + 1));

        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        http.setReuse(true);
        http.setTimeout(20000);

        if (http.begin(client, assetsManifestUrl))
        {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK)
            {
                manifest = http.getString();
                http.end();
                break;
            }
            http.end();
        }
        delay(1000 * (attempt + 1));  // Exponential backoff
    }

    if (manifest.length() == 0)
    {
        syslog_ng("Failed to download manifest");
        return false;
    }

    // Parse manifest
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, manifest);
    if (error)
    {
        syslog_ng("Failed to parse manifest: " + String(error.c_str()));
        return false;
    }

    // Check if files array exists
    if (!doc.containsKey("files") || !doc["files"].is<JsonArray>())
    {
        syslog_ng("Invalid manifest format");
        return false;
    }

    JsonArray files = doc["files"];
    syslog_ng("Found " + String(files.size()) + " files in manifest");

    // Download each file
    for (JsonVariant file : files)
    {
        if (!file.is<JsonObject>()) continue;

        const char* filePath = file["path"];
        if (!filePath) continue;

        String fullUrl = String(filePath);
        String localPath = "/assets/" + fullUrl.substring(fullUrl.lastIndexOf('/') + 1);

        syslog_ng("Downloading: " + fullUrl);
        if (!downloadAsset(fullUrl, localPath))
        {
            syslog_ng("Failed to download: " + fullUrl);
            // Continue with other files even if one fails
        }
    }

    // Download index.html.gz
    String indexUrl = baseUrl + "index.html.gz";
    syslog_ng("Downloading index file: " + indexUrl);
    if (!downloadAsset(indexUrl, "/index.html.gz"))
    {
        syslog_ng("Failed to download index file");
        return false;
    }

    return true;
}

bool doFirmwareUpdate()
{
    if (!downloadAndUpdateIndexFile())
    {
        syslog_ng("update: Failed to update index.html.gz");
        return false;
    }
    syslog_ng("update: downloadAndUpdateIndexFile done");
    HTTPClient http;
    if (!makeHttpRequest(UPDATE_URL, http))
    {
        syslog_ng("update: HTTP request failed.");
        return false;
    }
    totalLength = http.getSize();
    syslog_ng("update: Starting firmware update... totalLength " + String(totalLength));

    if (totalLength <= 0)
    {
        syslog_ng("update: Invalid firmware size");
        return false;
    }
    bool upd = Update.begin(totalLength, U_FLASH);
    syslog_ng("update: begin " + String(upd));

    WiFiClient* stream = http.getStreamPtr();
    int len = totalLength;
    int bytesDownloaded = 0;
    int lastPercentageSent = -1;
    unsigned long updateStartTime = millis();
    unsigned long lastProgressTime = millis();

    while (http.connected() && (len > 0 || len == -1))
    {
        if (millis() - updateStartTime > 900000)
        {  // 90 sec timeout
            syslog_ng("update: Total timeout exceeded");
            Update.abort();
            return false;
        }
        if (millis() - lastProgressTime > 10000)
        {  // 10 sec no progress
            syslog_ng("update: Progress timeout");
            Update.abort();
            return false;
        }

        size_t size = stream->available();
        if (size)
        {
            lastProgressTime = millis();
            int c = stream->readBytes(buff, min(size, sizeof(buff)));
            if (c <= 0)
            {
                syslog_ng("update: Read failed or no data");
                Update.abort();
                return false;
            }
            if (Update.write(buff, c) != c)
            {
                syslog_ng("update: Write failed c " + String(c));
                Update.abort();
                return false;
            }
            bytesDownloaded += c;
            if (len > 0) len -= c;

            int percentage = (bytesDownloaded * 100) / totalLength;
            if (percentage != lastPercentageSent)
            {
                syslog_ng("update: Updating: " + String(percentage) + "%");
                lastPercentageSent = percentage;
            }
            if (bytesDownloaded % 4096 == 0) vTaskDelay(1);
        }
        vTaskDelay(1);
    }
    if (len != 0)
    {
        syslog_ng("update: Size mismatch");
        Update.abort();
        return false;
    }
    if (!Update.end(true))
    {
        syslog_ng("update: Update end failed");
        return false;
    }

    syslog_ng("update: Update successful, rebooting...");
    preferences.putString(pref_reset_reason, "update");
    http.end();  // закрыть соединение
    preferences.putInt("upd", 0);
    delay(100);
    ESP.restart();
}

void make_update()
{
    if ((force_update) && UPDATE_URL && !making_update)
    {
        making_update = true;
        OtaStart = true;

        if (!doFirmwareUpdate())
        {
            // ошибка уже залогирована в doFirmwareUpdate
            making_update = false;
            OtaStart = false;
            percentage = 0;
        }
    }
    else
    {
        syslog_ng("update: auto-update skipping update due to conditions not met");
        making_update = false;
        OtaStart = false;
        percentage = 0;
    }
}

void TaskUP(void* param)
{
    // Если param - это request, можно от него отказываться при вызове из MQTT

    make_update();

    vTaskDelete(NULL);
}

void handleUpdate(AsyncWebServerRequest* request)
{
    preferences.putInt("upd", 1);
    syslog_ng("update: make_update start update firmware - wait 40-60 seconds, page will reload automatically");
    OtaStart = true;
    force_update = true;
    percentage = 1;
    handleRedirect(request);
    shouldReboot = true;
}
