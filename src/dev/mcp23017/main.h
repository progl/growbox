
uint8_t MCP23017addr = 0x20;

Adafruit_MCP23X17 mcp;

int pwd_val, pwd_freq, pwd_port;
int pwd_val2, pwd_freq2, pwd_port2;
int RootPwdMin, RootPwdMax, RootDistMin;
uint16_t bitw = 0;
int pwd_val_root, RootPwdOn, PwdStepUp, PwdStepDown, PwdDifPR, RootPwdTemp, MinKickPWD;

float PwdNtcRoot = -1;

bool isPumpOn = false;  // Флаг для отслеживания состояния помпы

unsigned long MCP23017_old = millis();
unsigned long MCP23017_Repeat = 10000;
unsigned long ECStabTimeStart = 0;
float aPWD = 0;
int DRV[DRV_COUNT][BITS_PER_DRV] = {{DRV1_A, DRV1_B, DRV1_C, DRV1_D},
                                    {DRV2_A, DRV2_B, DRV2_C, DRV2_D},
                                    {DRV3_A, DRV3_B, DRV3_C, DRV3_D},
                                    {DRV4_A, DRV4_B, DRV4_C, DRV4_D}};
void PwdPompKick(int PwdChannel, int PwdMax, int PwdStart, int PwdNorm, int KickTime)
{
    ledcWrite(PwdChannel, PwdStart);
    vTaskDelay(50);
    ledcWrite(PwdChannel, PwdMax);
    vTaskDelay(KickTime);
    ledcWrite(PwdChannel, PwdNorm);
}

#define MCP_RESET_PIN 23

void resetMCP23017()
{
    pinMode(MCP_RESET_PIN, OUTPUT);
    digitalWrite(MCP_RESET_PIN, LOW);   // Hold MCP23017 in reset
    vTaskDelay(100);                    // Hold reset for 100 ms
    digitalWrite(MCP_RESET_PIN, HIGH);  // Release reset
    mcp.begin_I2C();                    // Reinitialize I2C
}
