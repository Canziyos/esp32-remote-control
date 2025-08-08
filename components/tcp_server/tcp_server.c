#include "tcp_server.h"
#include "ota_handler.h"
#include "command_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 8080

static const char *TAG  = "TCP";
static const char *AUTH = "hunter2";

/**
 * @brief Handles a connected TCP client.
 *
 * Receives and processes one-line commands:
 *   - PING → responds with PONG
 *   - AUTH <token> → checks for valid session
 *   - led_on / led_off → triggers LED via command bus
 *   - OTA <size> <crc> → performs OTA update
 *
 * Terminates on client disconnect or error.
 *
 * @param arg Pointer casted from socket file descriptor (int).
 */
static void client_task(void *arg)
{
    int fd = (intptr_t)arg;
    char line[128];  // one-line input buffer

    for (;;) {
        int n = recv(fd, line, sizeof line - 1, 0);
        if (n <= 0) break;
        line[n] = '\0';

        if      (!strncmp(line, "PING", 4))           send(fd, "PONG\n", 5, 0);

        else if (!strncmp(line, "AUTH ", 5)) {
            if (!strcmp(line + 5, AUTH))              send(fd, "OK\n",     3, 0);
            else                                      send(fd, "DENIED\n", 7, 0);
        }

        else if (!strncmp(line, "led_on", 6)) {
            cmd_bus_send(CMD_LED_ON, 0);
            send(fd, "led_on\n", 7, 0);
        }

        else if (!strncmp(line, "led_off", 7)) {
            cmd_bus_send(CMD_LED_OFF, 0);
            send(fd, "led_off\n", 8, 0);
        }

        else if (!strncmp(line, "OTA ", 4)) {
            // OTA header format: "OTA <size> <crc32>"
            uint32_t sz, crc;
            if (sscanf(line + 4, "%u %x", (unsigned int *)&sz, (unsigned int *)&crc) == 2) {
                send(fd, "ACK\n", 4, 0);
                ota_perform(fd, sz);  // reboots if successful, never returns
            } else {
                send(fd, "BADFMT\n", 7, 0);
            }
        }

        else send(fd, "WHAT?\n", 6, 0);  // unknown command
    }

    shutdown(fd, 0);
    close(fd);
    vTaskDelete(NULL);
}

/**
 * @brief TCP server task.
 *
 * Opens a socket on PORT and listens for client connections.
 * Each accepted client spawns its own task (client_task).
 *
 * @param pv Unused.
 */
static void server_task(void *pv)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in a = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(s, (void *)&a, sizeof a);
    listen(s, 4);
    ESP_LOGI(TAG, "Listening on %d.", PORT);

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c >= 0)
            xTaskCreate(client_task, "cli", 4096, (void *)(intptr_t)c, 5, NULL);
    }
}

/**
 * @brief Launches the TCP server.
 *
 * Call this once after Wi-Fi is initialized and connected.
 */
void launch_tcp_server(void) {
    xTaskCreate(server_task, "tcp_srv", 4096, NULL, 4, NULL);
}
