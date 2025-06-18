void loop()
{
    if (shouldReboot)
    {
        delay(500);  // дожидаемся отправки HTTP-ответа
        ESP.restart();
    }
}