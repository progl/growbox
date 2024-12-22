while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(AHT10addr), static_cast<uint8_t>(1));
if (Wire.available())
{
    myAHT10.softReset();
    delay(50);
    myAHT10.begin();
    myAHT10.setNormalMode();

    AHT10Params = {"AHT10", AHT10, 30000, xSemaphore_C};
    xTaskCreatePinnedToCore(TaskTemplate, "AHT10", stack_size, (void *)&AHT10Params, 1, NULL, 0);

    setSensorDetected("AHT10", 1);
}
xSemaphoreGive(xSemaphore_C);