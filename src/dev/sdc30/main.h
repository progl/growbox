
uint8_t SDC30addr = 0x61;

SCD30 airSensor;
unsigned long SDC30_old = millis();
unsigned long SDC30_Repeat = 30000;
