#include <Arduino.h>

#include "esp_system.h"
#include "esp_task_wdt.h"

#define STACK_DEPTH 4  // Ограничим глубину стека до 4 уровней

// Структура для хранения отладочной информации
typedef struct
{
    size_t heap_total;
    size_t heap_free;
    size_t heap_free_min;
    time_t heap_min_time;
    uint32_t backtrace[STACK_DEPTH];
} re_restart_debug_t;

// Переменная для хранения отладочной информации (сохраняется между
// перезагрузками)
__NOINIT_ATTR static re_restart_debug_t _debug_info;

// Функция для захвата трейса стека
void IRAM_ATTR debugBacktraceUpdate()
{
    // Захватим первые несколько уровней стека
    _debug_info.backtrace[0] = (uint32_t)__builtin_return_address(0);
#if STACK_DEPTH > 1
    _debug_info.backtrace[1] = (uint32_t)__builtin_return_address(1);
#endif
#if STACK_DEPTH > 2
    _debug_info.backtrace[2] = (uint32_t)__builtin_return_address(2);
#endif
#if STACK_DEPTH > 3
    _debug_info.backtrace[3] = (uint32_t)__builtin_return_address(3);
#endif
}

// Функция для обновления информации о куче
void IRAM_ATTR debugHeapUpdate()
{
    _debug_info.heap_total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    _debug_info.heap_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t _new_free_min = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    if ((_debug_info.heap_free_min == 0) || (_new_free_min < _debug_info.heap_free_min))
    {
        _debug_info.heap_free_min = _new_free_min;
        _debug_info.heap_min_time = time(nullptr);
    }
}

// Функция для обновления всей отладочной информации
void IRAM_ATTR debugUpdate()
{
    debugHeapUpdate();
    debugBacktraceUpdate();
}

// Обертка для функции esp_panic_handler, которая будет вызвана при панике
extern "C" void __real_esp_panic_handler(void *info) __attribute__((noreturn));
extern "C" void __wrap_esp_panic_handler(void *info)
{
    debugUpdate();  // Сохранение отладочной информации перед вызовом оригинального
                    // обработчика паники

    // Вызов оригинального обработчика паники для завершения обработки ошибки
    __real_esp_panic_handler(info);
}

// Функция для вывода сохраненной отладочной информации
void printDebugInfo()
{
    Serial.println("Heap Info:");
    Serial.printf("Total heap: %u\n", _debug_info.heap_total);
    Serial.printf("Free heap: %u\n", _debug_info.heap_free);
    Serial.printf("Minimum free heap: %u at time %u\n", _debug_info.heap_free_min, _debug_info.heap_min_time);

    Serial.println("Backtrace:");
    size_t length = sizeof(_debug_info.backtrace) / sizeof(_debug_info.backtrace[0]);
    for (size_t i = 0; i < length; ++i)
    {
        if (_debug_info.backtrace[i] == 0) break;  // Если 0 — конец стека.
        Serial.printf("0x%08x\n", _debug_info.backtrace[i]);
    }
}

String getDebugInfoAsString()
{
    String debugInfo = "";

    debugInfo += "Heap Info:\n";
    debugInfo += "Total heap: " + String(_debug_info.heap_total) + "\n";
    debugInfo += "Free heap: " + String(_debug_info.heap_free) + "\n";
    debugInfo += "Minimum free heap: " + String(_debug_info.heap_free_min) + " at time " +
                 String(_debug_info.heap_min_time) + "\n";

    debugInfo += "Backtrace:\n";
    for (int i = 0; i < 4; ++i)
    {
        if (_debug_info.backtrace[i] == 0) break;
        debugInfo += "0x" + String(_debug_info.backtrace[i], HEX) + "\n";
    }

    return debugInfo;
}
