
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);



PRParams = {"PR", PR_void, 30000, xSemaphore_C};
xTaskCreatePinnedToCore(TaskTemplate, "PR", stack_size, (void *)&PRParams, 1, NULL, 1);

setSensorDetected("PR", 1);
xSemaphoreGive(xSemaphore_C);