
void TaskHX710B()
{
    uint32_t data_raw = 0;
    long cont = 0;
    long HX710B_err = HX710B_BUSY;
    while (HX710B_err != HX710B_OK and cont < 30)
    {
        HX710B_err = HX710B_Press.read(&data_raw, 1000UL);
        cont++;
        vTaskDelay(100);
    }
    if (data_raw != 0)
    {
        HX710B_DistRM.add(HX710B_a + float(data_raw) * HX710B_b);
        Dist = HX710B_DistRM.getAverage(2);
        DstRAW = data_raw;
        publish_parameter("Dist", Dist, 3, 1);
        syslog_ng("HX710B Dist:" + fFTS(Dist, 4));
        syslog_ng("HX710B Pressure RAW:" + fFTS(DstRAW, 0) + " cont:" + fFTS(cont, 0));
    }
    else
        syslog_ng("HX710B Error. Please check sensor");
}
TaskParams TaskHX710BParams;