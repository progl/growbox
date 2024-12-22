
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(MCP3421addr), static_cast<uint8_t>(1));
if (Wire.available())
{
    MCP342x::generalCallReset();
    delay(1);  // MC342x needs 300us to settle
    MCP3421Params = {"MCP3421", MCP3421, 30000, xSemaphore_C};
    xTaskCreatePinnedToCore(TaskTemplate, "MCP3421", stack_size, (void *)&MCP3421Params, 1, NULL, 1);

    setSensorDetected("MCP3421", 1);
}

xSemaphoreGive(xSemaphore_C);