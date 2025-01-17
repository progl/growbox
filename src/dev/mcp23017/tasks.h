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
    if (variablePointers["PWD1"] != nullptr && variablePointers["FREQ1"] != nullptr)
    {
        int &pwd_val = *variablePointers["PWD1"];
        int &pwd_freq = *variablePointers["FREQ1"];
        start_ledc_pwd(PwdChannel1, pwd_val, pwd_freq, PWDport1, "PWD1");
    }
    else
    {
        // Логирование ошибки в случае отсутствия указателя
        if (variablePointers["PWD1"] == nullptr)
        {
            syslog_err("MCP23017 Pointer to PWD1 is null!");
        }
        if (variablePointers["FREQ1"] == nullptr)
        {
            syslog_err("MCP23017 Pointer to FREQ1 is null!");
        }
    }
}

void start_ledc_pwd2()
{
    int &pwd_val2 = *variablePointers["PWD2"];    // Ссылка на переменную PWD2
    int &pwd_freq2 = *variablePointers["FREQ2"];  // Значение переменной FREQ2
    start_ledc_pwd(PwdChannel2, pwd_val2, pwd_freq2, PWDport2, "PWD2");
}

void MCP23017()
{
    int t = 0;
    bitw = 0b00000000;
    readGPIO = mcp.readGPIOAB();

    if (pwd_val != PWD1 || pwd_freq != FREQ1 || pwd_port != PWDport1)
    {
        pwd_val = PWD1;
        pwd_freq = FREQ1;
        pwd_port = PWDport1;
        start_ledc_pwd1();
    }

    if (pwd_val2 != PWD2 || pwd_freq2 != FREQ2 || pwd_port2 != PWDport2)
    {
        pwd_val2 = PWD2;
        pwd_freq2 = FREQ2;
        pwd_port2 = PWDport2;
        start_ledc_pwd2();
    }

    // Адаптивная циркуляция для снижения скачков корневого давления
    if (RootPomp == 1)
    {
        syslog_ng("MCP23017 RootPomp");

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
                        t = preferences.putInt("KickOnce", 1);
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
            t = preferences.putInt("PWD2", pwd_val_root);
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
            t = preferences.putInt((options[SelectedRP] + "_State").c_str(), pumpState);
            // Преобразование String в std::string перед конкатенацией
            std::string key = std::string(options[SelectedRP].c_str()) + "_State";

            *variablePointers[key] = pumpState;

            if (t == 0)
            {
                syslog_ng("MCP23017 error put " + options[SelectedRP] + "_State");
            }
        }
    }

    // Коррекция ЕС путем разбавления
    if (ECStabEnable == 1 && wEC > ECStabValue && 
    millis() - ECStabTimeStart > ECStabInterval * 1000 &&
        Dist >= ECStabMinDist && Dist < ECStabMaxDist

    )
    {
        syslog_ng("MCP23017 WATERING ECStabTime " +String(ECStabTime) + " ECStabInterval " +String(ECStabInterval)
        );
        mcp.digitalWrite(ECStabPomp, 1);
        readGPIO = mcp.readGPIOAB();
        publish_parameter("readGPIO", readGPIO, 3, 1);
        vTaskDelay(ECStabTime * 1000);
        mcp.digitalWrite(ECStabPomp, 0);
        readGPIO = mcp.readGPIOAB();
        publish_parameter("readGPIO", readGPIO, 3, 1);
        ECStabOn += ECStabTime;
        ECStabTimeStart = millis();
        syslog_ng("MCP23017 END WATERING");
    }
    else
    {
        syslog_ng(
            "MCP23017 ECStabEnable " +  String(ECStabEnable == 1) + 
            " wEC > ECStabValue  " + String(wEC > ECStabValue) +
            " millis() - ECStabTimeStart  > ECStabInterval * 1000  " +
            " time: " +  String(millis() - ECStabTimeStart > ECStabInterval * 1000) + 
            " Dist >= ECStabMinDist  " + String(Dist >= ECStabMinDist) + 
            " Dist < ECStabMaxDist " + String(Dist < ECStabMaxDist));

        mcp.digitalWrite(ECStabPomp, 0);
    }

    bool min_light = wPR < MinLightLevel;

    // Остановка помпы ночью по датчику освещенности
    if (PompNightEnable == 1)
    {
        int pompState = min_light ? 0 : 1;
        if (PR != -1)
        {
            syslog_ng("MCP23017 PompNight enable");
            t = preferences.putInt((options[PompNightPomp] + "_State").c_str(), pompState);
            // Преобразование String в std::string перед конкатенацией
            std::string key = std::string(options[PompNightPomp].c_str()) + "_State";

            // Использование ключа для доступа к словарю variablePointers
            *variablePointers[key] = pompState;

            if (t == 0)
            {
                syslog_ng("MCP23017 error put PompNight_State");
            }
        }
        else
        {
            syslog_ng("MCP23017 PompNight STOP" + String(pompState));
            t = preferences.putInt((options[PompNightPomp] + "_State").c_str(), pompState);
            // Преобразование String в std::string перед конкатенацией
            std::string key = std::string(options[PompNightPomp].c_str()) + "_State";

            // Использование ключа для доступа к словарю variablePointers
            *variablePointers[key] = pompState;

            if (t == 0)
            {
                syslog_ng("MCP23017 error put PompNight_State");
            }
        }
    }

    // Периодическое управление помпой
    if ((RDWorkNight == 1 && min_light && RDEnable == 1) || (RDEnable == 1))
    {
        syslog_ng("MCP23017 PERIODICA START");
        if (millis() > NextRootDrivePwdOn)
        {
            t = preferences.putInt((options[RDSelectedRP] + "_State").c_str(), 1);
            // Преобразование String в std::string перед конкатенацией
            std::string key = std::string(options[RDSelectedRP].c_str()) + "_State";

            // Использование ключа для доступа к словарю variablePointers
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
        if (millis() > NextRootDrivePwdOff)
        {
            t = preferences.putInt((options[RDSelectedRP] + "_State").c_str(), 0);
            // Преобразование String в std::string перед конкатенацией
            std::string key = std::string(options[RDSelectedRP].c_str()) + "_State";

            // Использование ключа для доступа к словарю variablePointers
            *variablePointers[key] = 0;

            if (t == 0)
            {
                syslog_ng("MCP23017 error put RDSelectedRPsate");
            }
            isPumpOn = false;
        }
    }
    else
    {
        NextRootDrivePwdOn = -1;
        NextRootDrivePwdOff = -1;
    }
    char bitString[17];  // строка для хранения битов, 16 бит + 1 символ для
                         // окончания строки

    // Управление драйверами
    for (int i = 0; i < 16; i++)
    {
        bitString[15 - i] = (readGPIO & (1 << i)) ? '1' : '0';
    }

    bitString[16] = '\0';  // добавляем символ окончания строки

    for (int i = 0; i < DRV_COUNT; i++)
    {
        for (int j = 0; j < BITS_PER_DRV; j++)
        {
            int currentPinState = *variablePointers[DRV_Keys[i][j].c_str()];
            bitWrite(bitw, DRV[i][j], currentPinState);
            if (log_debug)
                syslog_ng("MCP23017 READ:" + String(bitString) + " DRV" + String(i) + " pin " + String(DRV[i][j]) +
                          " " + String(DRV_Keys[i][j]) + " currentPinState " + String(currentPinState) + " bitRead " +
                          bitRead(readGPIO, DRV[i][j]) + " bitw " + bitRead(bitw, DRV[i][j]));

            if (bitRead(readGPIO, DRV[i][j]) == 0 && currentPinState == 1 &&
                *variablePointers[DRV_PK_On_Keys[i][j].c_str()] == 1)
            {
                int pwdChannel = (i < 2) ? PwdChannel1 : PwdChannel2;
                ledcSetup(pwdChannel, (i < 2) ? FREQ1 : FREQ2, PwdResolution1);
                ledcAttachPin((i < 2) ? PWDport1 : PWDport2, pwdChannel);
                PwdPompKick(pwdChannel, KickUpMax, KickUpStrart, (i < 2) ? PWD1 : PWD2, KickUpTime);
                ledcWrite(pwdChannel, (i < 2) ? PWD1 : PWD2);
                syslog_ng(String("MCP23017 PWD DRV" + String(i) + " pin " + String(DRV[i][j]) +
                                 " POMP KICK: executed! " + DRV_PK_On_Keys[i][j]));
            }
        }
    }
    if (readGPIO != bitw)
    {
        mcp.writeGPIOAB(bitw);
        // vTaskDelay(10);
        readGPIO = mcp.readGPIOAB();
    }

    // Проверка GPIO
    int GPIOerrcont = 0;
    if (readGPIO != bitw)
    {
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
                ESP.restart();
            }

            mcp.writeGPIOAB(bitw);
            readGPIO = mcp.readGPIOAB();

            if (log_debug)
            {
                char bitString[17];
                char bitString2[17];  // строка для хранения битов, 16 бит + 1 символ для
                                      // окончания строки

                // Управление драйверами
                for (int i = 0; i < 16; i++)
                {
                    bitString[15 - i] = (readGPIO & (1 << i)) ? '1' : '0';
                    bitString2[15 - i] = (bitw & (1 << i)) ? '1' : '0';
                }

                bitString[16] = '\0';   // добавляем символ окончания строки
                bitString2[16] = '\0';  // добавляем символ окончания строки

                syslog_ng("MCP23017 set  readGPIO " + String(bitString) + " bitw " + String(bitString2));
            }

            GPIOerrcont++;
            checkMismatchedPumps(readGPIO, bitw);
        }
    }
    // Публикация параметров
    publish_parameter("readGPIO", readGPIO, 3, 1);
}
TaskParams MCP23017Params;