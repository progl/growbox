while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

run_doser_nowParams = {"DOSER", run_doser_now, 30000, xSemaphore_C};
addTask(&run_doser_nowParams);

xSemaphoreGive(xSemaphore_C);