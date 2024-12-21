
void ADS1115_void()
{

  pHraw = adc.getRawResult();

  long cont = 0;
  double sensorValue = 0;
  while (cont < ADS1115_MiddleCount)
  {
    cont++;
    sensorValue = adc.getResult_mV() + sensorValue;
  }
  pHmV = sensorValue / cont;

  syslog_ng("ADS1115 pHmV:" + fFTS(pHmV, 3));
}
  TaskParams ADS1115Params ;