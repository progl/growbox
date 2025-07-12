while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(AM2320addr), static_cast<uint8_t>(1));
if (Wire.available())
{
    if (!AM2320.begin())
    {
        Serial.println("AM2320 Sensor not found");
    }
    TaskAM2320Params = {"TaskAM2320", TaskAM2320, 30000, xSemaphore_C};

    addTask(&TaskAM2320Params);

    setSensorDetected("AM2320", 1);
}
xSemaphoreGive(xSemaphore_C);