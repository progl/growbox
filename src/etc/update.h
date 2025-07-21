extern void handleRedirect(AsyncWebServerRequest* request);

// Global HTTP client instances for better memory management
extern HTTPClient http;
extern WiFiClientSecure wifiClient;
bool createDirectories(String path)
{
    if (path.startsWith("/"))
    {
        path = path.substring(1);  // Убираем начальный слеш
    }

    String currentPath = "";
    int start = 0;

    while (true)
    {
        int end = path.indexOf('/', start);
        String part;

        if (end == -1)
        {
            part = path.substring(start);  // последний сегмент
        }
        else
        {
            part = path.substring(start, end);
        }

        currentPath += "/" + part;

        if (!LittleFS.exists(currentPath))
        {
            if (!LittleFS.mkdir(currentPath))
            {
                syslog_ng("mkdir failed: " + currentPath);
                return false;
            }
            else
            {
                syslog_ng("mkdir ok: " + currentPath);
            }
        }

        if (end == -1) break;  // последний уровень
        start = end + 1;
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
    // Configure global client instances
    wifiClient.setInsecure();
    wifiClient.setHandshakeTimeout(10000);  // 10 second timeout
    http.setTimeout(15000);                 // 15 second timeout
    http.setUserAgent("GrowBox/1.0");
    http.setReuse(true);

    syslog_ng("downloadAsset: " + url);
    // Get file size first
    if (!http.begin(wifiClient, url.c_str()))
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
    if (contentLength <= 0) int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        syslog_ng("Invalid content length");
        http.end();
        return false;
    }

    // Check free space (add some extra margin)
    if (!checkFreeSpace(contentLength + 1024))
    {
        syslog_ng("Not enough free space to download " + url);
        http.end();
        return false;
    }

    // Create parent directories
    int lastSlash = localPath.lastIndexOf('/');
    if (lastSlash > 0)
    {
        String dirPath = localPath.substring(0, lastSlash);
        if (!createDirectories(dirPath))
        {
            syslog_ng("Failed to create directory: " + dirPath);
            http.end();
            return false;
        }
    }

    // Delete existing file if it exists
    if (LittleFS.exists(localPath))
    {
        closeAllOpenFiles();
        LittleFS.remove(localPath);
        syslog_ng("Removed existing file: " + localPath);
    }

    // Open file for writing
    File file = LittleFS.open(localPath, "w");
    if (!file)
    {
        syslog_ng("Failed to create file: " + localPath);
        http.end();
        return false;
    }

    // Get the stream
    WiFiClient* stream = http.getStreamPtr();
    if (!stream)
    {
        syslog_ng("Failed to get HTTP stream");
        file.close();
        http.end();
        return false;
    }

    // Download file
    size_t totalRead = 0;
    uint8_t buffer[256];  // Reduced buffer size to save memory
    while (http.connected() && totalRead < (size_t)contentLength)
    {
        size_t size = stream->available();
        if (size)
        {
            int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
            if (c > 0)
            {
                size_t written = file.write(buffer, c);
                if (written != (size_t)c)
                {
                    syslog_ng("Write failed: " + String(written) + " bytes written, expected " + String(c));
                    file.close();
                    http.end();
                    return false;
                }
                totalRead += written;
                yield();  // Let other tasks run

                // Let other tasks run
                if (totalRead % 1024 == 0)
                {
                    taskYIELD();
                }
            }
        }
        delay(1);
    }

    file.close();
    http.end();
    wifiClient.stop();  // Explicitly close the secure client

    if (totalRead != (size_t)contentLength)
    {
        syslog_ng("Download incomplete: " + String(totalRead) + " of " + String(contentLength));
        LittleFS.remove(localPath);
        return false;
    }

    syslog_ng("Successfully downloaded: " + localPath + " (" + String(totalRead) + " bytes)");
    return true;
}

bool formatLittleFS()
{
    Serial.println("Formatting LittleFS...");

    // Unmount first if mounted
    if (LittleFS.begin())
    {
        LittleFS.end();
    }

    // Format the filesystem
    bool formatted = LittleFS.format();
    if (!formatted)
    {
        Serial.println("LittleFS format failed!");
        return false;
    }

    // Verify the format by mounting
    if (!LittleFS.begin(true))
    {
        Serial.println("Failed to mount LittleFS after format!");
        return false;
    }

    Serial.println("LittleFS formatted successfully");
    return true;
}

void clearLittleFS()
{
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
    bool success = false;

    // Clear filesystem first
    clearLittleFS();

    // Configure global clients
    wifiClient.setInsecure();
    wifiClient.setHandshakeTimeout(10000);

    // Download manifest
    String manifest;
    for (int attempt = 0; attempt < maxRetries && !success; attempt++)
    {
        syslog_ng("Downloading manifest, attempt " + String(attempt + 1));

        // Configure HTTP client for this attempt
        http.setReuse(false);
        http.setTimeout(15000);
        http.setUserAgent("GrowBox/1.0");
        configureHttpClientForFirefox(http);

        if (http.begin(wifiClient, assetsManifestUrl.c_str()))
        {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK)
            {
                manifest = http.getString();
                syslog_ng("Successfully downloaded manifest");
                success = true;
            }
            else
            {
                syslog_ng("HTTP GET failed with code: " + String(httpCode));
            }
        }
        else
        {
            syslog_ng("Failed to connect to " + assetsManifestUrl);
        }
        http.end();
        // wifiClient.stop();

        if (!success)
        {
            delay(1000 * (attempt + 1));  // Exponential backoff
        }
    }

    if (!success)
    {
        syslog_ng("Failed to download manifest after " + String(maxRetries) + " attempts");
        return false;
    }

    // Allocate JSON document on the stack with explicit size
    StaticJsonDocument<4096> doc;  // зарезервировать память
    DeserializationError error = deserializeJson(doc, manifest);
    if (error)
    {
        syslog_ng("Failed to parse manifest: " + String(error.c_str()));
        return false;
    }

    if (!doc["files"].is<JsonArray>())
    {
        syslog_ng("Invalid manifest format: missing or invalid 'files' array");
        return false;
    }

    JsonArray files = doc["files"].as<JsonArray>();
    syslog_ng("Found " + String(files.size()) + " files in manifest");

    // Download each file
    for (JsonVariant file : files)
    {
        String filePath = file["fs_path"].as<String>();
        String fullUrl = file["path"].as<String>();

        // Ensure fsPath starts with a single slash and doesn't contain query parameters
        String fsPath = filePath;

        syslog_ng("Downloading: " + fullUrl);
        syslog_ng("Saving to: " + fsPath);

        if (!downloadAsset(fullUrl, fsPath))
        {
            syslog_ng("Failed to download: " + fullUrl);
        }

        // Add small delay between downloads
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Download index.html.gz
    success = downloadAsset(baseUrl + "index.html.gz", "/index.html.gz");

    // Clean up clients
    http.end();
    // wifiClient.stop();

    return success;
}

bool doFirmwareUpdate()
{
    HTTPClient http;
    configureHttpClientForFirefox(http);
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
        vTaskDelay(10);
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
    return true;
}

bool doFilesystemUpdate()
{
    HTTPClient http;
    configureHttpClientForFirefox(http);

    if (!makeHttpRequest(UPDATE_FS_URL, http))
    {
        syslog_ng("update: HTTP request for filesystem update failed.");
        return false;
    }

    totalLength = http.getSize();
    syslog_ng("update: Starting filesystem update... totalLength " + String(totalLength));

    if (totalLength <= 0)
    {
        syslog_ng("update: Invalid filesystem image size");
        return false;
    }

    // Begin the update with U_SPIFFS for LittleFS
    bool upd = Update.begin(totalLength, U_SPIFFS);
    syslog_ng("update: begin filesystem update " + String(upd));
    if (!upd)
    {
        syslog_ng("update: Not enough space for filesystem update");
        return false;
    }

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
            syslog_ng("update: Filesystem update timeout");
            Update.abort();
            return false;
        }
        if (millis() - lastProgressTime > 10000)
        {  // 10 sec no progress
            syslog_ng("update: Filesystem update progress timeout");
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
                syslog_ng("update: Filesystem read failed or no data");
                Update.abort();
                return false;
            }
            if (Update.write(buff, c) != c)
            {
                syslog_ng("update: Filesystem write failed, written " + String(c));
                Update.abort();
                return false;
            }
            bytesDownloaded += c;
            if (len > 0) len -= c;

            int percentage = (bytesDownloaded * 100) / totalLength;
            if (percentage != lastPercentageSent)
            {
                syslog_ng("update: Updating filesystem: " + String(percentage) + "%");
                lastPercentageSent = percentage;
            }
            if (bytesDownloaded % 4096 == 0) vTaskDelay(1);
        }
        vTaskDelay(10);
    }

    if (len != 0)
    {
        syslog_ng("update: Filesystem size mismatch");
        Update.abort();
        return false;
    }

    if (!Update.end(true))
    {
        syslog_ng("update: Filesystem update end failed: " + String(Update.getError()));
        return false;
    }

    syslog_ng("update: Filesystem update successful, rebooting...");
    preferences.putString(pref_reset_reason, "fs_update");
    http.end();
    preferences.putInt("fs_upd", 0);
    return true;
}

void make_update()
{
    if ((force_update) && UPDATE_URL && !making_update)
    {
        making_update = true;
        OtaStart = true;
        bool fw_update = false;
        bool fs_update = false;

        if (doFirmwareUpdate())
        {
            fw_update = true;
            syslog_ng("update: firmware update successful");
        }
        if (doFilesystemUpdate())
        {
            fs_update = true;
            syslog_ng("update: filesystem update successful");
        }
        if (fw_update || fs_update)
        {
            ESP.restart();
        }
        making_update = false;
        OtaStart = false;
        percentage = 0;
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
