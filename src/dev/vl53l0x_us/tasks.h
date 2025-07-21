
void TaskVL53L0X()
{
    Wire.begin(US_SDA, US_SCL);
    delay(100);
    long err = 0;
    float range = 0;
    float range0 = 0;
    unsigned cont = 0;
    unsigned long t = millis();

    while (millis() - t < 5000)
    {
        // range0 = s_VL53L0X.readRangeSingleMillimeters();
        range0 = s_VL53L0X.readRangeContinuousMillimeters();
        if (s_VL53L0X.timeoutOccurred())
        {
            syslogf("VL53L0X: TIMEOUT");
        }
        if (range0 < 8000)
        {
            VL53L0X_RangeRM.add(range0);
            cont++;
        }
        else
            err++;
    }

    range = VL53L0X_RangeRM.getMedian() / 10;
    VL53L0X_RangeAVG.add(range);

    Dist = VL53L0X_RangeAVG.getAverage();
    if (abs(Dist - range) > 4)
    {
        VL53L0X_RangeAVG.clear();
        syslogf("VL53L0X: Reset average filter");
    }

    publish_parameter("Dist", Dist, 3, 1);
    syslogf("VL53L0X: dist=%sms", fFTS(Dist, 3));
    syslogf("VL53L0X: Error/Count %s/%s", String(err), String(cont));
    syslogf("VL53L0X: Highest= %sms", fFTS(VL53L0X_RangeRM.getHighest(), 1));
    syslogf("VL53L0X: Lowest= %sms", fFTS(VL53L0X_RangeRM.getLowest(), 1));
    Wire.begin(I2C_SDA, I2C_SCL);
}
TaskParams TaskVL53L0XParams;