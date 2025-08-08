#pragma once

/**
 * @brief Launches the TCP server task.
 *
 * Creates a listening socket on port 8080 and handles incoming
 * TCP connections in separate tasks. Call this once after Wi-Fi
 * has successfully connected.
 */
void launch_tcp_server(void);
