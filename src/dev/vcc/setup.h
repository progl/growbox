while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

TaskvccParams = {"Vcc", Taskvcc, 30000, xSemaphore_C};
addTask(&TaskvccParams);
xSemaphoreGive(xSemaphore_C);