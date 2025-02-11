

void make_update_send_response()
{
    server.sendHeader("Location", "/");
    server.send(302);
}
void handleRoot()
{
    syslog_ng("WEB /root");

    if (server.arg("make_update") != "" and server.arg("make_update").toInt() == 1)
    {
    }

    sendFileLittleFS("/index.html");
}
