#define EC_DigitalPort1 18
#define EC_DigitalPort2 19
#define EC_AnalogPort GPIO_NUM_33  // gpio33
#define EC_MiddleCount 20000       // 60000 за 1 сек
unsigned long EC_old = millis();
unsigned long EC_Repeat = 10000;
int avg_number = 1000;
RunningMedian ApRM_AVG = RunningMedian(3);
RunningMedian AnRM_AVG = RunningMedian(3);