while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

TaskCPUTEMPParams = {"CPUTEMP", TaskCPUTEMP, 30000, xSemaphore_C};
xTaskCreatePinnedToCore(TaskTemplate, "CPUTEMP", stack_size, (void *)&TaskCPUTEMPParams, 1, NULL, 0);
xSemaphoreGive(xSemaphore_C);