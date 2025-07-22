
extern "C"
{
    uint8_t temprature_sens_read();
}

// Функция для считывания температуры
void TaskCPUTEMP()
{
    bool success = false;

    uint8_t raw = temprature_sens_read();
    CPUTemp = (raw - 32) / 1.8f;
    success = (raw != 128);

    if (success)
    {
        syslogf("CPU Temperature: %.2f", CPUTemp);
        publish_parameter("CPUTemp", CPUTemp, 3, 1);
    }
    else
    {
        syslogf("Failed to read CPU temperature");
    }
}

TaskParams TaskCPUTEMPParams;
