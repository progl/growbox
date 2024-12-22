while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

run_doser_nowParams = {"DOSER", run_doser_now, 30000, xSemaphore_C};
xTaskCreatePinnedToCore(TaskTemplate, "DOSER", stack_size, (void *)&run_doser_nowParams, 1, NULL, 1);

xSemaphoreGive(xSemaphore_C);