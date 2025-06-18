
void TaskSDC30()
{
    if (airSensor.dataReady())
    {
        if (!airSensor.read())
        {
            syslog_ng("SDC30: Read failed");
            return;
        }
        CO2 = airSensor.CO2;
        AirTemp = airSensor.temperature;
        AirHum = airSensor.relative_humidity;
        syslog_ng("SDC30 CO2 (ppm): " + fFTS(CO2, 3));
        syslog_ng("SDC30 AirTemp: " + fFTS(AirTemp, 3));
        syslog_ng("SDC30 AirHum: " + fFTS(AirHum, 3));

        publish_parameter("AirTemp", AirTemp, 3, 1);
        publish_parameter("AirHum", AirHum, 3, 1);
        publish_parameter("CO2", CO2, 3, 1);
    }
    else
    {
        syslog_ng("SDC30 Error - restart");
        airSensor.begin();
    }
}
TaskParams TaskSDC30Params;