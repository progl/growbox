
void loop()
{
    server.handleClient();
    ArduinoOTA.handle();
    delay(1); // Задержка на 1 секунду
}  