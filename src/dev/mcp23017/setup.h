while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
  ;

if (!mcp.begin_I2C())
{
  syslog_ng("mcp23017 Begin Error.");
  setSensorDetected("MCP23017", 0);
}
else
{
  syslog_ng("mcp23017 Begin.");

  for (int i = 0; i < 16; i++)
  {
    mcp.pinMode(i, OUTPUT); // Настройка всех пинов как выходы
  }
  mcp.writeGPIOAB(0); // Установка всех пинов в низкий уровень

  MCP23017Params = {"MCP23017", MCP23017, 30000, xSemaphore_C};
  xTaskCreatePinnedToCore(TaskTemplate,
                          "MCP23017",
                          stack_size,
                          (void *)&MCP23017Params,
                          1,
                          NULL,
                          0);

  setSensorDetected("MCP23017", 1);
}

xSemaphoreGive(xSemaphore_C);