
void TaskSDC30()
{
    if (airSensor.dataReady())
    {
        if (!airSensor.read())
        {
            syslogf("SDC30: Read failed");
            return;
        }
        CO2 = airSensor.CO2;
        AirTemp = airSensor.temperature;
        AirHum = airSensor.relative_humidity;
        syslogf("SDC30 CO2 (ppm): %.3f", CO2);
        syslogf("SDC30 AirTemp: %.3f", AirTemp);
        syslogf("SDC30 AirHum: %.3f", AirHum);

        publish_parameter("AirTemp", AirTemp, 3, 1);
        publish_parameter("AirHum", AirHum, 3, 1);
        publish_parameter("CO2", CO2, 3, 1);
    }
    else
    {
        syslogf("SDC30 Error - restart");
        airSensor.begin();
    }
}
TaskParams TaskSDC30Params;