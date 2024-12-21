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
    rootVPD = calculateVPD(RootTemp, AirHum);
    airVPD = calculateVPD(AirTemp, AirHum);
    syslog_ng("AM2320 AirTemp " + fFTS(AirTemp, 0));
    syslog_ng("AM2320 AirHum " + fFTS(AirHum, 0));
  }
  else
  {
    delay(2000);
    int status = AM2320.read();
    Serial.println(status);
    syslog_ng("AM2320 error " + fFTS(status, 0));
  }
}
TaskParams TaskAM2320Params;