#include <lwip/sockets.h>
#include <sys/time.h>
#include <sys/param.h>
#include "ota_handler.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_crc.h"          // IDF ≥5 has esp_crc32_le()
#include <errno.h>
#include <sys/socket.h>

static const char *TAG = "OTA";
#define CHUNK_SZ 1024
#define RECV_TIMEOUT_MS 8000      // 8-second stall guard

/**
 * @brief Receive and write OTA binary to next available partition.
 *
 * This function receives a binary file over a TCP socket, verifies its CRC32,
 * writes it to the next OTA slot, sets the boot partition, and triggers reboot.
 *
 * @param client_fd  Connected socket to the TCP client sending firmware.
 * @param claimed    Expected total size of the binary in bytes (from the header).
 * 
 * @return ESP_OK if the OTA succeeded and reboot triggered (never returns),
 *         otherwise an ESP_ERR_* code.
 */
esp_err_t ota_perform(int client_fd, uint32_t claimed)
{
    // --- Set socket receive timeout to avoid stalling forever ---
    struct timeval tv = {
        .tv_sec  = RECV_TIMEOUT_MS / 1000,
        .tv_usec = (RECV_TIMEOUT_MS % 1000) * 1000
    };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    // --- Prepare OTA partition ---
    const esp_partition_t *dst = esp_ota_get_next_update_partition(NULL);
    if (!dst) {
        ESP_LOGE(TAG,"No OTA partition");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG,"Updating %s (%u bytes)", dst->label, claimed);

    esp_ota_handle_t h;
    ESP_ERROR_CHECK_WITHOUT_ABORT( esp_ota_begin(dst, claimed, &h) );

    uint8_t  buf[CHUNK_SZ];
    size_t   total = 0;
    uint32_t crc   = 0;  // running CRC32-LE

    // --- Receive binary in chunks and write to flash ---
    while (total < claimed) {
        int n = recv(client_fd, buf, MIN(CHUNK_SZ, claimed - total), 0);
        if (n <= 0) {
            ESP_LOGE(TAG,"recv err/stall %d", errno);
            esp_ota_abort(h);
            return ESP_FAIL;
        }
        crc   = esp_crc32_le(crc, buf, n);
        total += n;
        ESP_ERROR_CHECK_WITHOUT_ABORT( esp_ota_write(h, buf, n) );
    }

    // --- Expect trailing CRC32 from sender (4 bytes) ---
    uint32_t sent_crc = 0;
    if (recv(client_fd, &sent_crc, sizeof sent_crc, MSG_WAITALL) != 4) {
        ESP_LOGE(TAG,"CRC not received");
        esp_ota_abort(h);
        return ESP_FAIL;
    }

    // --- Verify CRC match ---
    if (sent_crc != crc) {
        ESP_LOGE(TAG,"CRC mismatch (calc 0x%08x vs sent 0x%08x)", crc, sent_crc);
        esp_ota_abort(h);
        return ESP_ERR_INVALID_CRC;
    }

    // --- Finalize and reboot ---
    ESP_ERROR_CHECK_WITHOUT_ABORT( esp_ota_end(h) );
    ESP_ERROR_CHECK_WITHOUT_ABORT( esp_ota_set_boot_partition(dst) );
    ESP_LOGI(TAG,"OTA OK => reboot");

    esp_restart();  // Never returns

    return ESP_OK;
}
