// main.c – Wi-Fi debug build (12 KB task, scans APs at startup)

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "command_bus.h"
#include "led.h"
#include "tcp_server.h"

// Station credentials
#define STA_SSID       "TN_wifi_D6C00D"  // or "ViceHot"
#define STA_PASSWORD   "7WDFEWGTNM"      // or "12345678"

static const char *TAG = "WIFI-TASK";
static esp_netif_t *sta_netif;

/**
 * @brief Maps Wi-Fi event ID to a readable string.
 */
static const char *wifi_event_name(int32_t id)
{
    switch (id) {
        case WIFI_EVENT_WIFI_READY:          return "WIFI_READY";
        case WIFI_EVENT_SCAN_DONE:           return "SCAN_DONE";
        case WIFI_EVENT_STA_START:           return "STA_START";
        case WIFI_EVENT_STA_STOP:            return "STA_STOP";
        case WIFI_EVENT_STA_CONNECTED:       return "STA_CONNECTED";
        case WIFI_EVENT_STA_DISCONNECTED:    return "STA_DISCONNECTED";
        case WIFI_EVENT_STA_AUTHMODE_CHANGE: return "AUTHMODE_CHANGE";
        default:                             return "OTHER";
    }
}

/**
 * @brief Called when STA receives an IP.
 *
 * Launches the TCP server once the device is connected and has IP.
 */
static void got_ip(void *arg, esp_event_base_t b, int32_t id, void *data)
{
    ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
    launch_tcp_server();
}

/**
 * @brief Generic Wi-Fi event handler.
 *
 * Handles STA start, disconnection, and logs all other events.
 */
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ESP_LOGI(TAG, "Event %s (%ld)", wifi_event_name(id), id);

    if (id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "Disconnected (reason=%d) – reconnecting …", d->reason);
        esp_wifi_connect();
    }
}

/**
 * @brief Wi-Fi task: full station setup, blocking AP scan, waits for IP.
 *
 * This task:
 *  - Initializes NVS, netif, and event loop.
 *  - Starts Wi-Fi driver with logging and event handlers.
 *  - Performs a one-time scan for visible APs (blocking).
 *  - Suspends itself afterward; connection continues via event handlers.
 */
static void wifi_task(void *arg)
{
    // NVS, netif, event loop
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    // Driver setup and logging
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_log_level_set("*", ESP_LOG_VERBOSE);  // everything verbose

    // Country code and allowed channels
    wifi_country_t ctry = {
        .cc = "US", .schan = 1, .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    esp_wifi_set_country(&ctry);

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip, NULL, NULL));

    // Configure credentials
    wifi_config_t sta_cfg = {0};
    strcpy((char *)sta_cfg.sta.ssid, STA_SSID);
    strcpy((char *)sta_cfg.sta.password, STA_PASSWORD);
    sta_cfg.sta.threshold.authmode =
        strlen(STA_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    // Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi driver started – scanning …");

    // Blocking AP scan
    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    esp_wifi_scan_get_ap_num(&ap_num);

    wifi_ap_record_t *recs = heap_caps_malloc(ap_num * sizeof(*recs), MALLOC_CAP_DEFAULT);
    if (recs &&
        esp_wifi_scan_get_ap_records(&ap_num, recs) == ESP_OK) {
        for (int i = 0; i < ap_num; ++i)
            ESP_LOGI(TAG, "%2d | %-32s CH=%2d RSSI=%d",
                     i, recs[i].ssid, recs[i].primary, recs[i].rssi);
    }
    free(recs);

    ESP_LOGI(TAG, "Waiting for connection and IP events …");
    vTaskSuspend(NULL);  // suspend task but keep it alive
}

/**
 * @brief Application entry point.
 *
 * Initializes the command bus and LED task,
 * then starts Wi-Fi in a separate task (12 KB stack).
 */
void app_main(void)
{
    cmd_bus_init();
    led_task_start();

    xTaskCreatePinnedToCore(
        wifi_task,
        "wifi_task",
        12 * 1024,   // 12 KB stack
        NULL,
        5,
        NULL,
        0            // run on core 0
    );
}
