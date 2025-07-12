while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

TaskCPUTEMPParams = {"CPUTEMP", TaskCPUTEMP, 30000, xSemaphore_C};
addTask(&TaskCPUTEMPParams);
xSemaphoreGive(xSemaphore_C);