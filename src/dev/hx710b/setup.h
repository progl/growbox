
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

uint32_t HX710B_probe = -1;
if (HX710B_Press.init()) HX710B_Press.read(&HX710B_probe, 1000UL);

if (HX710B_probe != -1)
{
    TaskHX710BParams = {"TaskHX710B", TaskHX710B, 30000, xSemaphore_C};
    xTaskCreatePinnedToCore(TaskTemplate, "TaskHX710B", stack_size, (void *)&TaskHX710BParams, 1, NULL, 0);

    setSensorDetected("HX710B", 1);
}

xSemaphoreGive(xSemaphore_C);