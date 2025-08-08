#pragma once
#include <stdint.h>
#include <sys/socket.h>

/**
 * @brief Perform OTA update from a connected client.
 *
 * @param client_fd     Socket descriptor of the connected client.
 * @param claimed_size  Total expected size of the firmware image.
 *
 * This function receives the new firmware over TCP, verifies it using CRC32,
 * and writes it to the next OTA partition. On success, it reboots the device.
 *
 * @return esp_err_t    ESP_OK on success, or error code on failure.
 */
esp_err_t ota_perform(int client_fd, uint32_t claimed_size);
