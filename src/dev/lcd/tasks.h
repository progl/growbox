
void TaskLCD()
{
    syslogf("LCD update: 1");
    oled.clear();
    oled.home();
    oled.setScale(3);
    char buffer[32];
    oled.println("UPTIME");
    if (fFTS(millis() / 1000, 0, buffer, sizeof(buffer)))
    {
        oled.println(buffer);
    }
    oled.setScale(2);
    if (fFTS(PR, 0, buffer, sizeof(buffer)))
    {
        oled.print("PR:");
        oled.println(buffer);
    }
    oled.update();

    if (AirTemp or AirHum or CO2 or AirPress)
    {
        syslogf("LCD update: 2");

        oled.clear();
        oled.home();
        oled.setScale(2);
        oled.println("Воздух:");
        oled.setScale(2);
        if (AirTemp)
        {
            if (fFTS(AirTemp, 1, buffer, sizeof(buffer)))
            {
                oled.print("Темп:");
                oled.println(buffer);
            }
        }
        if (AirHum)
        {
            if (fFTS(AirHum, 1, buffer, sizeof(buffer)))
            {
                oled.print("Влаж:");
                oled.print(buffer);
                oled.println("%");
            }
        }
        if (CO2)
        {
            if (fFTS(CO2, 0, buffer, sizeof(buffer)))
            {
                oled.print("CO2:");
                oled.println(buffer);
            }
        }
        if (AirPress)
        {
            if (fFTS(AirPress, 1, buffer, sizeof(buffer)))
            {
                oled.print("Давл:");
                oled.println(buffer);
            }
        }
        oled.update();
    }

    if ((wEC and !isnan(wEC)) or (wpH and !isnan(wpH)) or (wNTC and !isnan(wNTC)))
    {
        syslogf("LCD update: 3");
        oled.clear();
        oled.home();
        oled.setScale(2);
        oled.println("Раствор:");
        oled.setScale(2);
        if (wEC and !isnan(wEC))
        {
            if (fFTS(wEC, 3, buffer, sizeof(buffer)))
            {
                oled.print("ЕС:");
                oled.print(buffer);
                oled.println("mS");
            }
        }
        if (wpH and !isnan(wpH))
        {
            if (fFTS(wpH, 3, buffer, sizeof(buffer)))
            {
                oled.print("pH:");
                oled.println(buffer);
            }
        }
        if (wNTC and !isnan(wNTC))
        {
            if (fFTS(wNTC, 2, buffer, sizeof(buffer)))
            {
                oled.print("Темп:");
                oled.println(buffer);
            }
        }
        oled.update();
    }
}
TaskParams TaskLCDParams;