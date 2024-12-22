while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);
rtc_clk_slow_freq_set(RTC_SLOW_FREQ_RTC);
rtc_clk_fast_freq_set(RTC_FAST_FREQ_XTALD4);
rtc_cpu_freq_config_t freq_config;
rtc_clk_cpu_freq_get_config(&freq_config);
TaskCPUTEMPParams = {"CPUTEMP", TaskCPUTEMP, 30000, xSemaphore_C};
xTaskCreatePinnedToCore(TaskTemplate, "CPUTEMP", stack_size, (void *)&TaskCPUTEMPParams, 1, NULL, 0);
xSemaphoreGive(xSemaphore_C);