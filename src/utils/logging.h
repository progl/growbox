#pragma once

#include <Arduino.h>

// Simple logging macros
#define SYSLOG_TAG "GrowBox"

// Logging levels
typedef enum
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;

// Function declarations
void log_printf(log_level_t level, const char* tag, const char* format, ...);
void log_init();

// Logging macros
#define LOG_E(tag, format, ...) log_printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define LOG_W(tag, format, ...) log_printf(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define LOG_I(tag, format, ...) log_printf(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define LOG_D(tag, format, ...) log_printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define LOG_V(tag, format, ...) log_printf(LOG_LEVEL_VERBOSE, tag, format, ##__VA_ARGS__)

// Backward compatibility
#define syslog_ng(fmt, ...) LOG_I(SYSLOG_TAG, fmt, ##__VA_ARGS__)
