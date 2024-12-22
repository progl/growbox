void TaskCPUTEMP()
{
    const int numSensors = 2;  // Количество температурных сенсоров

    // Чтение данных с датчиков и добавление в RunningMedian
    for (int i = 0; i < 6; i++)
    {
        CpuTempRM[0].add(readTemp1(true));
        CpuTempRM[1].add(readTemp2(true));
    }

    // Обновление среднего значения для каждого сенсора
    for (int i = 0; i < numSensors; i++)
    {
        CpuTempRM_AVG[i].add(CpuTempRM[i].getAverage(5));
    }

    // Вычисление общего среднего значения для обоих сенсоров и сохранение в третий RunningMedian
    float combinedAverage = (CpuTempRM_AVG[0].getAverage(5) + CpuTempRM_AVG[1].getAverage(5)) / 2;
    CpuTempRM_AVG[2].add(combinedAverage);

    // Получение финального значения для CPUTemp
    CPUTemp = CpuTempRM_AVG[2].getAverage(5);

    // Логирование и публикация результатов
    syslog_ng("CPUTemp-Core1:" + fFTS(CpuTempRM_AVG[0].getAverage(5), 3));
    syslog_ng("CPUTemp-Core2:" + fFTS(CpuTempRM_AVG[1].getAverage(5), 3));
    syslog_ng("CPUTemp:" + fFTS(CPUTemp, 3));

    publish_parameter("CPUTemp", CPUTemp, 3, 1);
    publish_parameter("CPUTemp1", CpuTempRM_AVG[0].getAverage(5), 3, 1);
    publish_parameter("CPUTemp2", CpuTempRM_AVG[1].getAverage(5), 3, 1);
}
TaskParams TaskCPUTEMPParams;
