void Taskvcc()
{
    Vcc = rom_phy_get_vdd33() / 2048;
    syslog_ng("Vcc=" + fFTS(Vcc, 3));
    publish_parameter("Vcc", Vcc, 3, 1);
}

TaskParams TaskvccParams;
