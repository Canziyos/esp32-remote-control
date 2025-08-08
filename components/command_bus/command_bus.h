#pragma once

#include "freertos/FreeRTOS.h"    // included first.
#include "freertos/queue.h"

// Command types used across tasks via the internal message queue.
typedef enum {
    CMD_LED_ON,       // Turn LED on.
    CMD_LED_OFF,      // Turn LED off.
    CMD_OTA_START     // OTA request (handled by TCP task, forwarded for clarity).
} cmd_t;

/**
 * @brief Initializes the internal command bus (FreeRTOS queue).
 * 
 * Call this once in app_main() before any send/receive calls.
 */
void cmd_bus_init(void);

/**
 * @brief Sends a command to the bus.
 * 
 * @param cmd   Command to send.
 * @param ticks Max number of ticks to wait if the queue is full.
 * @return pdTRUE on success, pdFALSE on timeout.
 */
BaseType_t cmd_bus_send(cmd_t cmd, TickType_t ticks);

/**
 * @brief Receives a command from the bus.
 * 
 * @param out   Pointer to receive the command.
 * @param ticks Max number of ticks to wait if the queue is empty.
 * @return pdTRUE on success, pdFALSE on timeout.
 */
BaseType_t cmd_bus_receive(cmd_t *out, TickType_t ticks);
