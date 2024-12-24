
void loop()
{
    server.handleClient();
    webSocket.loop();
    ArduinoOTA.handle();
    delay(1);  // Задержка на 1 секунду
}