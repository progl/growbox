while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

TaskvccParams = {"Vcc", Taskvcc, 30000, xSemaphore_C};
xTaskCreatePinnedToCore(TaskTemplate, "Vcc", stack_size, (void *)&TaskvccParams, 1, NULL, 0);
xSemaphoreGive(xSemaphore_C);