
void TaskSDC30()
{

  if (airSensor.dataAvailable())
  {

    CO2 = airSensor.getCO2();
    AirTemp = airSensor.getTemperature();
    AirHum = airSensor.getHumidity();
    if (airSensor.getAutoSelfCalibration() == true)
      syslog_ng("SDC30 Warning: Enable Auto Self Calibration");
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
TaskParams TaskSDC30Params ;