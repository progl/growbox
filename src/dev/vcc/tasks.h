void Taskvcc()
{
  const int sampleCount = 64;

  // Собираем выборку значений Vcc
  for (int i = 0; i < sampleCount; i++)
  {
    vccRM.add(rom_phy_get_vdd33());
  }

  // Вычисляем среднее значение Vcc с учетом корректировки
  Vcc = vccRM.getAverage() / 2048 * VccAdj;

  // Логируем данные
  syslog_ng("Vcc=" + fFTS(Vcc, 3));
  syslog_ng("Vcc: Highest= " + fFTS(vccRM.getHighest(), 1));
  syslog_ng("Vcc: Lowest= " + fFTS(vccRM.getLowest(), 1));
  syslog_ng("Vcc: VccRAW= " + String(rom_phy_get_vdd33()));

  publish_parameter("Vcc", Vcc, 3, 1);
}

TaskParams TaskvccParams ;
