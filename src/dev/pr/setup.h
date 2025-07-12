
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

PRParams = {"PR", PR_void, 30000, xSemaphore_C};
addTask(&PRParams);

setSensorDetected("PR", 1);
xSemaphoreGive(xSemaphore_C);