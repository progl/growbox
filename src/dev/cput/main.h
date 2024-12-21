// Массивы для RunningMedian
RunningMedian CpuTempRM[3] = {RunningMedian(10), RunningMedian(10), RunningMedian(10)};
RunningMedian CpuTempRM_AVG[3] = {RunningMedian(10), RunningMedian(10), RunningMedian(10)};

// Константы калибровки
const float CALPOINT1_CELSIUS[] = {23.0f, 23.0f};
const float CALPOINT1_RAW[] = {128253742.0f, 163600.0f};
const float CALPOINT2_CELSIUS[] = {-20.0f, -20.0f};
const float CALPOINT2_RAW[] = {114261758.0f, 183660.0f};

float readTemp1(bool printRaw = false)
{
    uint64_t value = rtc_clk_cal_ratio(RTC_CAL_RTC_MUX, 100);
    return ((float)value - CALPOINT1_RAW[0]) * (CALPOINT2_CELSIUS[0] - CALPOINT1_CELSIUS[0]) / (CALPOINT2_RAW[0] - CALPOINT1_RAW[0]) + CALPOINT1_CELSIUS[0];
}

float readTemp2(bool printRaw = false)
{
    uint64_t startTime = rtc_time_get();
    delay(100); // Если возможно, уберите эту задержку или замените ее на что-то более подходящее
    uint64_t value = rtc_time_get() - startTime;
    return ((float)value * 10.0 - CALPOINT1_RAW[1]) * (CALPOINT2_CELSIUS[1] - CALPOINT1_CELSIUS[1]) / (CALPOINT2_RAW[1] - CALPOINT1_RAW[1]) + CALPOINT1_CELSIUS[1];
}
