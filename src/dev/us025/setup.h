if (change_pins == 1)
{
    US_ECHO = 14;
    US_TRIG = 13;
}
else
{
    US_ECHO = 13;
    US_TRIG = 14;
}
UltrasonicSensor distanceSensor(US_ECHO, US_TRIG, timeout);  // 30 мс таймаут

distanceSensor.begin();
distanceSensor.triggerPulse();
unsigned long start = millis();
while (!distanceSensor.waitForMeasurement() && (millis() - start < 1000))
{
}
distanceSensor.measureDistanceCm(measurement);

if (!isnan(measurement.distanceCm))
{  // Проверяем, что результат не NaN
    TaskUSParams = {"US", TaskUS, 30000, xSemaphore_C};
    int US025_TaskErr = xTaskCreatePinnedToCore(TaskTemplate, "US", stack_size, (void *)&TaskUSParams, 1, NULL, 1);
    setSensorDetected("US025", 1);
    syslog_ng("US025: определен");
}
else
{
    syslog_ng("US025: Результат измерения некорректный. ");
    setSensorDetected("US025", 0);
}
