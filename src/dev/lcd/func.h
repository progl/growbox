
void lcd(String x)
{
    oled.clear();

    oled.home();
    oled.println(x);
    oled.update();
}