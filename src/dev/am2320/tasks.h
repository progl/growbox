void TaskAM2320()
{
    if (AM2320.begin())
    {
        AirHum = AM2320.getHumidity();
        AirTemp = AM2320.getTemperature();

        AirTempRM.add(AirTemp);
        AirTemp = AirTempRM.getMedianAverage(3);
        AirHumRM.add(AirHum);
        AirHum = AirHumRM.getMedianAverage(3);

        publish_parameter("AirTemp", AirTemp, 3, 1);
        publish_parameter("AirHum", AirHum, 3, 1);
        Kornevoe = AirTemp - RootTemp;

        AirVPD = calculateVPD(AirTemp, AirHum);

        syslogf("AM2320 AirTemp  %.2f", AirTemp);
        syslogf("AM2320 AirHum %.2f", AirHum);
    }
    else
    {
        delay(2000);
        int status = AM2320.read();
        Serial.println(status);
        syslogf("AM2320 error %d", status);
    }
}
TaskParams TaskAM2320Params;