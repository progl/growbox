// Параметры шимов
void checkMismatchedPumps(uint16_t readGPIO, uint16_t bitw)
{
    String errorPumps = "";
    for (int i = 0; i < 16; i++)
    {
        uint16_t mask = 1 << i;
        if ((readGPIO & mask) != (bitw & mask))
        {
            errorPumps += "Pump " + String(i) + " mismatch, ";
        }
    }

    if (errorPumps.length() > 0)
    {
        syslog_err("MCP23017 Error set: checkMismatchedPumps readGPIO:" + String(readGPIO) +
                   " != writeGPIO:" + String(bitw) + "; Mismatched pumps: " + errorPumps);
    }
    else
    {
        syslog_ng("MCP23017: All pumps match correctly.");
    }
}

void start_ledc_pwd(int pwdChannel, int &pwd_val, int pwd_freq, int pwd_port, const char *pwd_name)
{
    syslog_ng(String("MCP23017 start_ledc_") + pwd_name + " PORT:" + String(pwd_port) + " VALUE:" + String(pwd_val) +
              " FREQ:" + String(pwd_freq));
    lcd(String(pwd_name) + "-" + String(pwd_freq) + "Hz:" + String(pwd_val));
    ledcSetup(pwdChannel, pwd_freq, PwdResolution1);
    ledcAttachPin(pwd_port, pwdChannel);
    publish_parameter(pwd_name, pwd_val, 3, 1);
    ledcWrite(pwdChannel, pwd_val);
}

void start_ledc_pwd1()
{
    // Проверка наличия указателя на переменную PWD1

    int &pwd_val = *variablePointers["PWD1"];
    int &pwd_freq = *variablePointers["FREQ1"];
    if (pwd_val == 0 || pwd_freq >= 255)
    {
        // Отключаем PWM
        ledcWrite(PwdChannel1, 0);  // Устанавливаем значение 0 для отключения
        ledcDetachPin(PwdChannel1);
    }
    else
    {
        start_ledc_pwd(PwdChannel1, pwd_val, pwd_freq, PWDport1, "PWD1");
    }
}

void start_ledc_pwd2()
{
    int &pwd_val2 = *variablePointers["PWD2"];    // Ссылка на переменную PWD2
    int &pwd_freq2 = *variablePointers["FREQ2"];  // Значение переменной FREQ2
    if (pwd_val2 == 0 || pwd_freq2 < 255)
    {
        // Отключаем PWM
        ledcWrite(PwdChannel2, 0);  // Устанавливаем значение 0 для отключения
        ledcDetachPin(PwdChannel2);
    }
    else
    {
        start_ledc_pwd(PwdChannel2, pwd_val2, pwd_freq2, PWDport2, "PWD2");
    }
}

// Глобальная переменная для управления состоянием
volatile bool isWatering = false;

// Функция для отдельного потока
static void wateringTask(void *params)
{
    std::string key = std::string(options[ECStabPomp].c_str()) + "_State";
    int *pumpPin = (int *)params;
    syslog_ng("Watering Task Started");

    // Включаем помпу
    mcp.digitalWrite(*pumpPin, 1);
    *variablePointers[key] = 1;
    publish_parameter(options[ECStabPomp], 1, 3, 1);

    // Ждем заданное время
    vTaskDelay(ECStabTime * 1000);

    // Выключаем помпу
    mcp.digitalWrite(*pumpPin, 0);
    *variablePointers[key] = 0;
    readGPIO = mcp.readGPIOAB();
    publish_parameter(options[ECStabPomp], 0, 3, 1);

    // Обновляем счетчики
    ECStabOn += ECStabTime;
    ECStabTimeStart = millis();
    syslog_ng("MCP23017 WATERING END WATERING");

    // Сбрасываем флаг
    isWatering = false;

    // Завершаем задачу
    vTaskDelete(NULL);
}

// Функция для управления корневым давлением
static void manageRootPressure()
{
    if (RootPomp == 1)
    {
        syslog_ng("MCP23017 RootPomp on");

        if (RootPwdOn == 1)
        {
            syslog_ng("MCP23017 RootPwdOn");
            bool condition1 = (RootPwdTemp == 1 && RootTemp > AirTemp && RootTemp > 15 && Dist > RootDistMin);
            bool condition2 = (PwdNtcRoot != 0 && RootTemp - wNTC < PwdNtcRoot);
            bool condition3 = (PwdDifPR > 0 && wPR > PwdDifPR) || (PwdDifPR == 0);

            if ((condition1 || condition2) && condition3)
            {
                pwd_val_root -= PwdStepDown;
            }
            else
            {
                if (PwdStepUp == 256)
                {
                    pwd_val_root = round((AirTemp - RootTemp) * 200);
                }
                else
                {
                    if (pwd_val_root <= MinKickPWD)
                    {
                        int t = preferences.putInt("KickOnce", 1);
                        *variablePointers["KickOnce"] = 1;

                        if (t == 0)
                        {
                            syslog_ng("MCP23017 error put KickOnce ");
                        }
                    }
                    pwd_val_root += PwdStepUp;
                }
            }

            pwd_val_root = constrain(pwd_val_root, RootPwdMin, RootPwdMax);
            int t = preferences.putInt("PWD2", pwd_val_root);
            *variablePointers["PWD2"] = pwd_val_root;
            if (t == 0)
            {
                syslog_ng("MCP23017 error put PWD2 ");
            }
        }
        else
        {
            // Управление включением/выключением помпы без ШИМ
            bool pumpState = (RootTemp > AirTemp && RootTemp > 15 && Dist > RootDistMin) ? 0 : 1;
            int t = preferences.putInt((options[SelectedRP] + "_State").c_str(), pumpState);
            std::string key = std::string(options[SelectedRP].c_str()) + "_State";
            *variablePointers[key] = pumpState;

            if (t == 0)
            {
                syslog_ng("MCP23017 error put " + options[SelectedRP] + "_State");
            }
        }
    }
}

static void managePompNight(bool min_light)
{
    if (PompNightEnable == 1)
    {
        int pompState = min_light ? 0 : 1;
        std::string key = std::string(options[PompNightPomp].c_str()) + "_State";
        std::string stateStr = std::to_string(pompState);

        if (*variablePointers[key] != pompState)
        {
            int t = preferences.putInt((options[PompNightPomp] + "_State").c_str(), pompState);
            *variablePointers[key] = pompState;

            if (t == 0)
            {
                syslog_ng("MCP23017 PompNight error put PompNight_State");
            }

            if (PR != -1)
            {
                syslog_ng("MCP23017 PompNight enable");
            }
            else
            {
                syslog_ng("MCP23017 PompNight STOP");
            }
        }
    }
}

static void managePWM(int &pwd_val, int &pwd_freq, int &pwd_port, int pwd_val_new, int pwd_freq_new, int pwd_port_new,
                      int channel)
{
    if (pwd_val != pwd_val_new || pwd_freq != pwd_freq_new || pwd_port != pwd_port_new)
    {
        pwd_val = pwd_val_new;
        pwd_freq = pwd_freq_new;
        pwd_port = pwd_port_new;

        if (channel == 1)
        {
            start_ledc_pwd1();
        }
        else
        {
            start_ledc_pwd2();
        }
    }
}

static void managePeriodicPump(bool min_light)
{
    if ((RDWorkNight == 1 && min_light && RDEnable == 1) || (RDEnable == 1))
    {
        if (log_debug)
            syslog_ng("MCP23017 PERIODICA START + RDWorkNight " + String(RDWorkNight == 1) + " min_light " +
                      String(min_light) + " RDEnable " + String(RDEnable == 1));

        if (millis() > NextRootDrivePwdOn)
        {
            std::string key = std::string(options[RDSelectedRP].c_str()) + "_State";
            if (*variablePointers[key] != 1)
            {
                int t = preferences.putInt((options[RDSelectedRP] + "_State").c_str(), 1);
                *variablePointers[key] = 1;

                if (t == 0)
                {
                    syslog_ng("MCP23017 error put RDSelectedRP state");
                }
                t = preferences.putInt("PWD2", RDPWD);
                *variablePointers["PWD2"] = RDPWD;
                if (t == 0)
                {
                    syslog_ng("MCP23017 error put RDPWD");
                }
                NextRootDrivePwdOff = millis() + (RDDelayOn * 1000);
                NextRootDrivePwdOn = NextRootDrivePwdOff + (RDDelayOff * 1000);
                isPumpOn = true;
            }
        }

        if (millis() > NextRootDrivePwdOff)
        {
            std::string key = std::string(options[RDSelectedRP].c_str()) + "_State";
            if (*variablePointers[key] != 0)
            {
                int t = preferences.putInt((options[RDSelectedRP] + "_State").c_str(), 0);
                *variablePointers[key] = 0;

                if (t == 0)
                {
                    syslog_ng("MCP23017 error put RDSelectedRPsate");
                }
                isPumpOn = false;
            }
        }
    }
    else
    {
        NextRootDrivePwdOn = -1;
        NextRootDrivePwdOff = -1;
    }
}

static void updateDriverStates()
{
    // Формируем строку для логирования
    char bitString[17];
    bitString[16] = '\0';
    for (int i = 0; i < 16; i++)
    {
        bitString[15 - i] = (readGPIO & (1 << i)) ? '1' : '0';
    }

    // Обновляем состояние каждого драйвера
    for (int i = 0; i < DRV_COUNT; i++)
    {
        for (int j = 0; j < BITS_PER_DRV; j++)
        {
            String drv_key = DRV_Keys[i][j].c_str();
            int currentPinState = *variablePointers[drv_key.c_str()];
            byte cur_bitw_state = bitRead(readGPIO, DRV[i][j]);
            bitWrite(bitw, DRV[i][j], currentPinState);

            // Выполняем пинок при необходимости
            if (cur_bitw_state == 0 && currentPinState == 1 && *variablePointers[DRV_PK_On_Keys[i][j].c_str()] == 1)
            {
                int pwdChannel = (i < 2) ? PwdChannel1 : PwdChannel2;
                int freq = (i < 2) ? FREQ1 : FREQ2;
                int pwd = (i < 2) ? PWD1 : PWD2;

                if (freq > 0 && pwd < 255 && pwd > 0)
                {
                    ledcSetup(pwdChannel, freq, PwdResolution1);
                    ledcAttachPin((i < 2) ? PWDport1 : PWDport2, pwdChannel);
                    PwdPompKick(pwdChannel, KickUpMax, KickUpStrart, pwd, KickUpTime);
                    ledcWrite(pwdChannel, pwd);
                    syslog_ng(String("MCP23017 PWD DRV" + String(i) + " pin " + String(DRV[i][j]) +
                                     " POMP KICK: executed! " + DRV_PK_On_Keys[i][j] + " FREQ: " + String(freq) +
                                     " PWD: " + String(pwd)));
                }
                else
                {
                    syslog_ng(String("MCP23017 PWD DRV" + String(i) + " pin " + String(DRV[i][j]) +
                                     " POMP KICK: skipped! FREQ: " + String(freq) + " PWD: " + String(pwd)));
                }
            }
        }
    }
}

static void handleGPIOErrors()
{
    if (readGPIO != bitw)
    {
        int GPIOerrcont = 0;
        while (readGPIO != bitw && GPIOerrcont < 25)
        {
            if (GPIOerrcont == 10)
            {
                syslog_ng("MCP23017 10 resetMCP23017");
                resetMCP23017();
            }
            if (GPIOerrcont == 20)
            {
                syslog_ng("MCP23017 20 resetMCP23017");
                resetMCP23017();
                preferences.putString(pref_reset_reason, "mcp");
                shouldReboot = true;
            }

            mcp.writeGPIOAB(bitw);
            readGPIO = mcp.readGPIOAB();

            // Логируем текущее состояние
            char bitString[17];
            char bitString2[17];
            bitString[16] = '\0';
            bitString2[16] = '\0';
            for (int i = 0; i < 16; i++)
            {
                bitString[15 - i] = (readGPIO & (1 << i)) ? '1' : '0';
                bitString2[15 - i] = (bitw & (1 << i)) ? '1' : '0';
            }
            syslog_ng("MCP23017 WRITE readGPIO " + String(readGPIO) + " bitw " + String(bitw));
            vTaskDelay(1);
            GPIOerrcont++;
        }
        checkMismatchedPumps(readGPIO, bitw);
    }
}

static void publishPinStates()
{
    for (int i = 0; i < 16; i++)
    {
        uint16_t mask = 1 << i;
        uint8_t status = (readGPIO & mask) ? 1 : 0;
        publish_parameter(options[i], status, 3, 1);
    }
}
void MCP23017()
{
    int t = 0;
    int GPIOerrcont = 0;
    bitw = 0b00000000;
    readGPIO = mcp.readGPIOAB();
    bool min_light = wPR < MinLightLevel;
    char bitString[17];    // строка для хранения битов, 16 бит + 1 символ для
                           // окончания строки
    bitString[16] = '\0';  // добавляем символ окончания строки

    // Управление настройками PWM
    managePWM(pwd_val, pwd_freq, pwd_port, PWD1, FREQ1, PWDport1, 1);
    managePWM(pwd_val2, pwd_freq2, pwd_port2, PWD2, FREQ2, PWDport2, 2);
    // Адаптивная циркуляция для снижения скачков корневого давления
    manageRootPressure();
    if (log_debug)
        syslog_ng("MCP23017 WATERING ECStabEnable==1 " + String(ECStabEnable == 1) + " wEC > ECStabValue  " +
                  String(wEC > ECStabValue) + " millis() - ECStabTimeStart  > ECStabInterval * 1000  " + " time: " +
                  String(millis() - ECStabTimeStart > ECStabInterval * 1000) + " wLevel >= ECStabMinDist  " +
                  String(wLevel >= ECStabMinDist) + " wLevel < ECStabMaxDist " + String(wLevel < ECStabMaxDist));
    // Коррекция ЕС путем разбавления
    if (ECStabEnable == 1 && wEC > ECStabValue && millis() - ECStabTimeStart > ECStabInterval * 1000 &&
        wLevel >= ECStabMinDist && wLevel < ECStabMaxDist)
    {
        syslog_ng("MCP23017 WATERING ECStabTime " + String(ECStabTime) + " ECStabInterval " + String(ECStabInterval));

        // Создаем задачу для полива с высшим приоритетом
        xTaskCreate(wateringTask, "Watering", 2048, &ECStabPomp, 2, NULL);
    }
    else
    {
        if (ECStabEnable == 1)
        {
            std::string key = std::string(options[ECStabPomp].c_str()) + "_State";

            bool isPumpActiveInGPIO = (readGPIO & (1 << ECStabPomp)) != 0;

            if (isPumpActiveInGPIO)
            {
                // Выключаем помпу только если она включена
                mcp.digitalWrite(ECStabPomp, 0);
                *variablePointers[key] = 0;
                readGPIO = mcp.readGPIOAB();  // Обновляем состояние после выключения
                syslog_ng("MCP23017 WATERING DISABLE ECStabEnable==1 " + String(ECStabEnable == 1) +
                          " wEC > ECStabValue  " + String(wEC > ECStabValue) +
                          " millis() - ECStabTimeStart  > ECStabInterval * 1000  " +
                          " time: " + String(millis() - ECStabTimeStart > ECStabInterval * 1000) +
                          " wLevel >= ECStabMinDist  " + String(wLevel >= ECStabMinDist) + " wLevel < ECStabMaxDist " +
                          String(wLevel < ECStabMaxDist));
            }
        }
    }

    // Остановка помпы ночью по датчику освещенности
    managePompNight(min_light);
    // Периодическое управление помпой
    managePeriodicPump(min_light);
    // Управление драйверами
    updateDriverStates();

    // Обновляем GPIO
    if (readGPIO != bitw)
    {
        syslog_ng("MCP23017 WRITE readGPIO: " + String(readGPIO) + " bitw: " + String(bitw));
        mcp.writeGPIOAB(bitw);
        vTaskDelay(1);
        readGPIO = mcp.readGPIOAB();
    }

    // Обработка ошибок GPIO
    handleGPIOErrors();

    // Публикация состояния пинов
    publishPinStates();
    readGPIO = mcp.readGPIOAB();
    syslog_ng("MCP23017  readGPIO: " + String(readGPIO));
}
TaskParams MCP23017Params;