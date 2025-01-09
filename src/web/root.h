void handleRoot()
{
    syslog_ng("WEB /root");

    if (server.arg("make_update") != "" and server.arg("make_update").toInt() == 1)
    {
        syslog_ng("handleRoot - make_update");
        OtaStart = true;
        force_update = true;
        percentage = 1;
        server.sendHeader("Location", "/");
        server.send(302);
        update_f();
    }

    sendFileSPIFF("index.html");



}
