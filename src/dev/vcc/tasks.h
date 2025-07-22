void Taskvcc()
{
    Vcc = rom_phy_get_vdd33() / 2048;
    syslogf("Vcc=%.3f", Vcc);
    publish_parameter("Vcc", Vcc, 3, 1);
}

TaskParams TaskvccParams;
