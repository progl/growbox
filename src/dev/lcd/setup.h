while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(LCDaddr), static_cast<uint8_t>(1));
if (Wire.available())
{
    TaskLCDParams = {"TaskLCD", TaskLCD, 30000, xSemaphore_C};
    addTask(&TaskLCDParams);

    setSensorDetected("LCD", 1);
}

xSemaphoreGive(xSemaphore_C);