
extern "C"
{
    uint8_t temprature_sens_read();
}

// Функция для считывания температуры
void TaskCPUTEMP()
{
    float CPUTemp = NAN;
    bool success = false;

    uint8_t raw = temprature_sens_read();
    CPUTemp = (raw - 32) / 1.8f;
    success = (raw != 128);

    if (success)
    {
        syslog_ng("CPU Temperature:  " + fFTS(CPUTemp, 2));
        publish_parameter("CPUTemp", CPUTemp, 3, 1);
    }
    else
    {
        syslog_ng("Failed to read CPU temperature\n");
    }
}

TaskParams TaskCPUTEMPParams;
