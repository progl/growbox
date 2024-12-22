void Taskvcc()
{
    // убрал 2048  тк у меня опказыва не 3.3  - надо вынести будет к юзеру
    Vcc = rom_phy_get_vdd33() / 1937.635;
    syslog_ng("Vcc=" + fFTS(Vcc, 3));
    publish_parameter("Vcc", Vcc, 3, 1);
}

TaskParams TaskvccParams;
