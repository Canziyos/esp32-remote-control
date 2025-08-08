// main.c – Wi-Fi debug build (12 KB task, OTA-safe)

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h" 
#include "command_bus.h"
#include "led.h"
#include "tcp_server.h"

/* ---------- user config ---------- */
#define STA_SSID       "my_ssid"
#define STA_PASSWORD   "my_password"
#define MAX_WIFI_RETRIES 5

/* ---------- globals ---------- */
static const char *TAG = "WIFI-TASK";
static esp_netif_t *sta_netif;
static int  wifi_retries     = 0;
static bool ota_verified     = false;

/* ---------- helpers ---------- */
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

/* ---------- IP handler: runs once we have an address ---------- */
static void got_ip(void *arg, esp_event_base_t b, int32_t id, void *data)
{
    ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));

    /* Mark this firmware as GOOD exactly once */
    if (!ota_verified) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            ESP_LOGI(TAG, "OTA verified - rollback cancelled.");
        else
            ESP_LOGW(TAG, "OTA valid-mark failed (already valid?)");
        ota_verified = true;
    }

    launch_tcp_server();
}

/* ---------- Wi-Fi event handler (generic) ---------- */
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ESP_LOGI(TAG, "Event %s (%ld)", wifi_event_name(id), id);

    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "Disconnected (reason=%d) - reconnecting …", d->reason);

        wifi_retries++;

        if (!ota_verified && wifi_retries >= MAX_WIFI_RETRIES) {
            ESP_LOGE(TAG, "Wi-Fi failed %d× => runtime rollback!", MAX_WIFI_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(250));           // flush logs
            esp_ota_mark_app_invalid_rollback_and_reboot(); // never returns
        }

        esp_wifi_connect();     // keep trying
    }
}

/* ---------- Wi-Fi task ---------- */
static void wifi_task(void *arg)
{
    /* NVS / netif / event loop */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    /* driver + verbose logging */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    /* country code (channels 1-11) */
    wifi_country_t ctry = {.cc = "SE", .schan = 1, .nchan = 11,
                           .policy = WIFI_COUNTRY_POLICY_AUTO};
    esp_wifi_set_country(&ctry);

    /* event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip, NULL, NULL));

    /* credentials */
    wifi_config_t sta_cfg = {0};
    strcpy((char *)sta_cfg.sta.ssid, STA_SSID);
    strcpy((char *)sta_cfg.sta.password, STA_PASSWORD);
    sta_cfg.sta.threshold.authmode =
        strlen(STA_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    /* start driver */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi driver started – waiting for IP …");

    vTaskSuspend(NULL);          // task sleeps forever; events drive the logic
}

/* ---------- entry point ---------- */
void app_main(void)
{
    cmd_bus_init();
    led_task_start();

    /* 12 KB stack, pin to core 0 */
    xTaskCreatePinnedToCore(
        wifi_task, "wifi_task",
        12 * 1024, NULL, 5, NULL, 0);
}
