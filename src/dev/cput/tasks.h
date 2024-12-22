
void TaskCPUTEMP()
{
    // Получаем температуру процессора
    CPUTemp = getCPUTemperature();

    // Добавляем в RunningMedian для усреднения
    CpuTempRM_AVG.add(CPUTemp);

    // Получаем финальное значение усредненной температуры
    float finalTemp = CpuTempRM_AVG.getAverage(5);  // Усреднение последних 5 значений

    // Логирование температуры
    syslog_ng("CPUTemp: " + fFTS(finalTemp, 3));

    // Публикация результата
    publish_parameter("CPUTemp", finalTemp, 3, 1);
}

TaskParams TaskCPUTEMPParams;
