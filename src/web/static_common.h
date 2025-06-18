#include "web/static.h"
server
    .serveStatic("/assets/fonts/bootstrap-icons.woff2", LittleFS, "/assets/fonts/bootstrap-icons.woff2",
                 "max-age=86400")
    .setCacheControl("max-age=86400")
    .setLastModified(false)
    .setTryGzipFirst(false);