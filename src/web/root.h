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
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    // Отправляем сжатый файл из PROGMEM
    server.send_P(200, "text/html", _Users_mmatveyev_PycharmProjects_web_calc_local_files_api_test_out_html_gz,
                  _Users_mmatveyev_PycharmProjects_web_calc_local_files_api_test_out_html_gz_len);
}
