const char *ca_cert = nullptr;

void configureHttpClientForFirefox(HTTPClient &http)
{
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:85.0) Gecko/20100101 Firefox/85.0");
    http.addHeader("Accept",
                   "text/html,application/xhtml+xml,application/"
                   "xml;q=0.9,image/webp,*/*;q=0.8");
    http.addHeader("Accept-Language", "en-US,en;q=0.5");
    http.addHeader("Accept-Encoding", "gzip, deflate, br");

    http.setTimeout(10000);
    http.setConnectTimeout(5000);
}

bool makeHttpRequest(String url, HTTPClient &http)
{
    syslogf("update: makeHttpRequest start");
    int retryCount = 0;
    const int maxRetries = 50;

    syslogf("update: makeHttpRequest ");
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

        syslogf("update: Response %d Retry %d http_begin %d resp %d UPDATE_URL %s", resp, retryCount, http_begin, resp,
                UPDATE_URL);
        if (resp == 200)
        {
            return true;  // Successful connection
        }
        else
        {
            syslogf("update:HTTP GET failed: resp %d error %s getSize %d", resp, http.errorToString(resp),
                    http.getSize());
        }
        retryCount++;
        http.end();
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Delay between attempts
    }

    syslogf("update: Failed to connect after %d retries", maxRetries);
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
