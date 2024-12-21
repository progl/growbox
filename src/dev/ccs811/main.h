
uint8_t CCS811addr = 0x5b;

CCS811 ccs811;

RunningMedian CCS811_eCO2RM = RunningMedian(4);
RunningMedian CCS811_tVOCRM = RunningMedian(4);
