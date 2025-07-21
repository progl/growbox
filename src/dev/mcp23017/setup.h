while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

if (!mcp.begin_I2C())
{
    syslogf("mcp23017 Begin Error.");
    setSensorDetected("MCP23017", 0);
}
else
{
    syslogf("mcp23017 Begin.");

    for (int i = 0; i < 16; i++)
    {
        mcp.pinMode(i, OUTPUT);  // Настройка всех пинов как выходы
    }
    mcp.writeGPIOAB(0);  // Установка всех пинов в низкий уровень

    MCP23017Params = {"MCP23017", MCP23017, 30000, xSemaphore_C};
    addTask(&MCP23017Params);

    setSensorDetected("MCP23017", 1);
}

xSemaphoreGive(xSemaphore_C);