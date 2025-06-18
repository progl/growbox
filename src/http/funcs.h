const char *ca_cert = nullptr;

void configureHttpClientForFirefox(HTTPClient &http)
{
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:85.0) Gecko/20100101 Firefox/85.0");
    http.addHeader("Accept",
                   "text/html,application/xhtml+xml,application/"
                   "xml;q=0.9,image/webp,*/*;q=0.8");
    http.addHeader("Accept-Language", "en-US,en;q=0.5");
    http.addHeader("Accept-Encoding", "gzip, deflate, br");

    http.setConnectTimeout(100000);
    http.setTimeout(65535);
}

void handleRedirect(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse(302);
    response->addHeader("Location", "/");  // Адрес, на который редирект
    request->send(response);
}

bool makeHttpRequest(String url, HTTPClient &http)
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
        if (!resp || resp == -1)
        {
            http_begin = http.begin(url);
            configureHttpClientForFirefox(http);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            resp = http.GET();
        }

        syslog_ng("update: Response " + String(resp) + " Retry " + String(retryCount) + " http_begin " +

                  String(http_begin) + " resp " + String(resp) + " UPDATE_URL " + String(UPDATE_URL));
        if (resp == 200)
        {
            return true;  // Successful connection
        }
        else
        {
            syslog_ng("update:HTTP GET failed: resp " + String(resp) + " error " + http.errorToString(resp) +
                      " getSize " + String(http.getSize()));
        }
        retryCount++;
        http.end();
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Delay between attempts
    }

    syslog_ng("update: Failed to connect after " + String(maxRetries) + " retries");
    return false;  // Connection failed
}

const char UPDATE_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<meta http-equiv="refresh" content="40;url=/">
start update firmware - wait 40-62 seconds, page will reload automatically
)=====";

void updatePage(AsyncWebServerRequest *request) { request->send(200, "text/html", UPDATE_page); }

const char ERROR_page[] PROGMEM = R"=====(
<!DOCTYPE html>
ERROR
)=====";

void errorPage(AsyncWebServerRequest *request) { request->send_P(400, "text/html", ERROR_page); }

String getContentType(const String &path)
{
    if (path.endsWith(".html"))
        return "text/html";
    else if (path.endsWith(".css"))
        return "text/css";
    else if (path.endsWith(".js"))
        return "application/javascript";
    else if (path.endsWith(".json"))
        return "application/json";
    else if (path.endsWith(".png"))
        return "image/png";
    else if (path.endsWith(".jpg"))
        return "image/jpeg";
    else if (path.endsWith(".gif"))
        return "image/gif";
    else if (path.endsWith(".ico"))
        return "image/x-icon";
    else if (path.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}
void handleFileRequest(AsyncWebServerRequest *request)
{
    if (request->hasArg("make_update") && request->arg("make_update").toInt() == 1)
    {
        handleRedirect(request);
        return;
    }

    String path = request->url();
    Serial.println("handleFileRequest path " + path);

    if (path == "" || path.endsWith("/"))
    {
        path += "index.html";
    }

    if (path == "/" && isAPMode)
    {
        path += "captive.html";
    }

    String contentType = getContentType(path);
    String filePath;
    bool isGzipped = false;

    // Проверка .gz файла
    if (FoundCache.count(+".gz") || !notFoundCache.count(path + ".gz"))
    {
        if (LittleFS.exists(path + ".gz"))
        {
            filePath = path + ".gz";
            isGzipped = true;
            FoundCache.insert(filePath);
        }
        else
        {
            notFoundCache.insert(path + ".gz");
        }
    }

    if (!notFoundCache.count(path))
    {
        if (FoundCache.count(path) || LittleFS.exists(path))
        {
            filePath = path;
            FoundCache.insert(path);
        }
        else
        {
            notFoundCache.insert(path);
        }
    }

    // Если файл найден
    if (!filePath.isEmpty())
    {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, filePath, contentType);
        if (isGzipped)
        {
            response->addHeader("Content-Encoding", "gzip");
        }
        response->addHeader("Cache-Control", "max-age=31536000, immutable");
        response->addHeader("Access-Control-Allow-Origin", "*");
        syslog_ng("send filePath " + filePath);
        request->send(response);
    }
    else
    {
        syslog_ng("send filePath " + filePath + " not found");
        request->send(404, "text/plain", "File Not Found");
    }
}
