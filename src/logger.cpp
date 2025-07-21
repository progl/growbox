// logger.cpp
#include "logger.h"
#include <stdarg.h>
void syslog_ng(String msg);
void syslogf(const char *fmt, ...)
{
    char buf[256];  // достаточно для большинства логов
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    syslog_ng(String(buf));  // или если syslog_ng принимает const char* – то без String
}
