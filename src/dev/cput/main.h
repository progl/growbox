RunningMedian CpuTempRM_AVG = RunningMedian(3);
float getCPUTemperature()
{
    // Чтение температуры с встроенного датчика процессора ESP32
    return temperatureRead();  // Возвращает температуру в градусах Цельсия
}