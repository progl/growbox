// #pragma once

// #include <Arduino.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>

// // Core assignments

// #define CORE_SENSORS 0

// // Task priorities (lower number = higher priority)
// #define PRIORITY_CRITICAL 1  // Web server, MQTT
// #define PRIORITY_HIGH 2      // Time-sensitive sensors
// #define PRIORITY_NORMAL 3    // Regular sensors
// #define PRIORITY_LOW 4       // Background tasks

// /**
//  * Determines the appropriate task priority based on task name and execution time
//  * @param taskName Name of the task
//  * @param executionTimeMs Average execution time in milliseconds
//  * @return UBaseType_t Priority level
//  */
// inline UBaseType_t getTaskPriority(const char* taskName, uint32_t executionTimeMs)
// {
//     // Critical tasks that must be responsive
//     if (strstr(taskName, "Web") != nullptr || strstr(taskName, "MQTT") != nullptr ||
//         strstr(taskName, "WiFi") != nullptr)
//     {
//         return PRIORITY_CRITICAL;
//     }

//     // Long-running tasks that should yield frequently
//     if (executionTimeMs > 100)
//     {                         // Tasks taking >100ms
//         return PRIORITY_LOW;  // Lower priority to prevent CPU hogging
//     }

//     // Time-sensitive sensor readings
//     if (strstr(taskName, "DS18B20") != nullptr || strstr(taskName, "BMP280") != nullptr ||
//         strstr(taskName, "AHT10") != nullptr)
//     {
//         return PRIORITY_HIGH;
//     }

//     // Default to normal priority
//     return PRIORITY_NORMAL;
// }

// /**
//  * Creates a task with error handling and logging
//  * @return true if task was created successfully, false otherwise
//  */
// inline bool createTask(TaskFunction_t taskFunction, const char* taskName, uint32_t stackSize, UBaseType_t priority,
//                        BaseType_t core, TaskHandle_t* taskHandle = nullptr)
// {
//     syslog_ng(String("Creating task: ") + taskName + " stack: " + String(stackSize) + " prio: " + String(priority) +
//               " core: " + String(core));

//     BaseType_t result = xTaskCreatePinnedToCore(taskFunction,  // Task function
//                                                 taskName,      // Name of task
//                                                 stackSize,     // Stack size in words
//                                                 NULL,          // Parameter passed as input to the task
//                                                 priority,      // Priority of the task
//                                                 taskHandle,    // Task handle
//                                                 core           // Core where the task should run
//     );

//     if (result != pdPASS)
//     {
//         syslog_ng(String("Failed to create task: ") + taskName);
//         return false;
//     }
//     syslog_ng(String("Successfully created task: ") + taskName);
//     return true;
// }
