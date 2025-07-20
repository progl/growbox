// Автоген. Не править руками
#include <ESPAsyncWebServer.h>
void serveFile(AsyncWebServerRequest* request, const String& path, const String& mime, bool gzip);
void serveFileWithGzip(AsyncWebServerRequest* request, const String& path, const String& mime, bool gzip);

void setupRoutes(AsyncWebServer& server) {
  server.on("/assets/imgs/smart_box.png", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/assets/imgs/smart_box.png", "image/png", true); });
  server.on("/assets/css/main-CjOEHTpB.css", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/assets/css/main-CjOEHTpB.css", "text/css", true); });
  server.on("/assets/js/main-Xtmb9hu9.js", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/assets/js/main-Xtmb9hu9.js", "application/javascript", true); });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/index.html", "text/html", true); });
  server.on("js-script", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/js-script.js", "application/javascript", true); });
  server.on("js-chart.umd.min", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/js-chart.umd.min.js", "application/javascript", true); });
  server.on("js-calibrate", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/js-calibrate.js", "application/javascript", true); });
  server.on("js-bootstrap.bundle.min", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/js-bootstrap.bundle.min.js", "application/javascript", true); });
 
        server.on("/assets/imgs/sprite.svg", HTTP_GET, [](AsyncWebServerRequest* request) { serveFileWithGzip(request, "/assets/imgs/sprite.svg", "image/svg+xml", true); });
        
}
