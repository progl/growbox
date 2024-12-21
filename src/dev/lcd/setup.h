while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
    ;

Wire.requestFrom(static_cast<uint16_t>(LCDaddr), static_cast<uint8_t>(1));
if (Wire.available())
{

    TaskLCDParams = {"TaskLCD", TaskLCD, 30000, xSemaphore_C};
    xTaskCreatePinnedToCore(TaskTemplate,
                            "TaskLCD",
                            stack_size,
                            (void *)&TaskLCDParams,
                            1,
                            NULL,
                            1);

    setSensorDetected("LCD", 1);
}

xSemaphoreGive(xSemaphore_C);