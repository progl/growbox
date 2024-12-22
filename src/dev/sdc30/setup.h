while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(SDC30addr), static_cast<uint8_t>(1));
if (Wire.available())
{
    if (airSensor.begin() == false)  // Pass the Wire port to the .begin() function
    {
        syslog_ng("SDC30: Air sensor not detected.");
    }

    TaskSDC30Params = {"TaskSDC30", TaskSDC30, 30000, xSemaphore_C};
    xTaskCreatePinnedToCore(TaskTemplate, "TaskSDC30", stack_size, (void *)&TaskSDC30Params, 1, NULL, 0);

    setSensorDetected("SDC30", 1);
}

xSemaphoreGive(xSemaphore_C);