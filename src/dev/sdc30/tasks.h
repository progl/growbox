
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
        syslogf("SDC30 CO2 (ppm): %sms", fFTS(CO2, 3));
        syslogf("SDC30 AirTemp: %sms", fFTS(AirTemp, 3));
        syslogf("SDC30 AirHum: %sms", fFTS(AirHum, 3));

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