
void TaskLCD()
{
    syslog_ng("LCD update: 1");
    oled.clear();
    oled.home();
    oled.setScale(3);
    oled.println("UPTIME");
    oled.println(fFTS(millis() / 1000, 0));
    oled.setScale(2);
    oled.println("PR:" + fFTS(PR, 0));
    oled.update();

    if (AirTemp or AirHum or CO2 or AirPress)
    {
        syslog_ng("LCD update: 2");

        oled.clear();
        oled.home();
        oled.setScale(2);
        oled.println("Воздух:");
        oled.setScale(2);
        if (AirTemp) oled.println("Темп:" + fFTS(AirTemp, 1));
        if (AirHum) oled.println("Влаж:" + fFTS(AirHum, 1) + "%");
        if (CO2) oled.println("CO2:" + fFTS(CO2, 0));
        if (AirPress) oled.println("Давл:" + fFTS(AirPress, 1));
        oled.update();
    }

    if ((wEC and !isnan(wEC)) or (wpH and !isnan(wpH)) or (wNTC and !isnan(wNTC)))
    {
        syslog_ng("LCD update: 3");
        oled.clear();
        oled.home();
        oled.setScale(2);
        oled.println("Раствор:");
        oled.setScale(2);
        if (wEC and !isnan(wEC)) oled.println("ЕС:" + fFTS(wEC, 3) + "mS");
        if (wpH and !isnan(wpH)) oled.println("pH:" + fFTS(wpH, 3));
        if (wNTC and !isnan(wNTC)) oled.println("Темп:" + fFTS(wNTC, 2));
        oled.update();
    }
}
TaskParams TaskLCDParams;