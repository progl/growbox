// logger.cpp
#include "logger.h"
#include <stdarg.h>
void syslog_ng(String msg);
void syslogf(const char *fmt, ...)
{
    static char buf[512];  // достаточно для большинства логов
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    syslog_ng(buf);  // или если syslog_ng принимает const char* – то без String
}
