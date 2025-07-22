
void TaskVL6180X()
{
    syslogf("VL6180X Range:%dms", s_vl6180X.readRangeContinuousMillimeters());

    s_vl6180X.setScaling(vl6180XScalling);
    // int32_t rangemm = s_vl6180X.readRangeSingleMillimeters();
    int32_t rangemm = s_vl6180X.readRangeContinuousMillimeters();

    if (rangemm < 250) vl6180XScalling = 1;
    if (rangemm >= 250 and rangemm < 500) vl6180XScalling = 2;
    if (rangemm >= 500) vl6180XScalling = 3;

    s_vl6180X.setScaling(vl6180XScalling);

    delay(100);
    long err = 0;
    float range0 = 0;
    unsigned cont = 0;
    unsigned long t = millis();
    while (millis() - t < 5000)
    {
        // s_vl6180X.timeoutOccurred();
        range0 = s_vl6180X.readRangeContinuousMillimeters();
        if (range0 != 768)
        {
            VL6180X_RangeRM.add(range0);
            cont++;
        }
        else
        {
            syslog_err("VL6180X: Error range");
            err++;
            Wire.begin();
        }
    }

    VL6180X_RangeAVG.add(VL6180X_RangeRM.getMedian() / 10);
    Dist = VL6180X_RangeAVG.getAverage();

    syslogf("VL6180X: dist=%.2fms range=%ldmm Scalling=%d Error/Count=%lu/%lu Highest=%.1fms Lowest=%.1fms", Dist,
            rangemm, vl6180XScalling, err, cont, VL6180X_RangeRM.getHighest(), VL6180X_RangeRM.getLowest());

    publish_parameter("Dist", Dist, 3, 1);
}
TaskParams TaskVL6180XParams;