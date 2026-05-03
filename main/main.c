#include <stdbool.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "ipma_monitor.h"
#include "nvs_flash.h"

static const char *TAG = "app_main";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < CONFIG_IPMA_WIFI_MAX_RETRY) {
            s_retry_count++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "Retrying Wi-Fi connection (%d/%d)",
                     s_retry_count, CONFIG_IPMA_WIFI_MAX_RETRY);
            return;
        }

        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_connect(void)
{
    if (strlen(CONFIG_IPMA_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "Wi-Fi SSID is empty. Configure it via menuconfig first.");
        return ESP_ERR_INVALID_STATE;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id_instance;
    esp_event_handler_instance_t got_ip_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &any_id_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &got_ip_instance));

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = CONFIG_IPMA_WIFI_MAX_RETRY,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    strlcpy((char *)wifi_config.sta.ssid, CONFIG_IPMA_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_IPMA_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password));

    if (strlen(CONFIG_IPMA_WIFI_PASSWORD) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Wi-Fi connection failed");
    return ESP_FAIL;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(wifi_connect());

    ipma_monitor_config_t monitor_config = {
        .dico = CONFIG_IPMA_MONITOR_DICO,
        .http_timeout_ms = CONFIG_IPMA_MONITOR_HTTP_TIMEOUT_MS,
    };

    ESP_ERROR_CHECK(ipma_monitor_init(&monitor_config));

    ipma_monitor_result_t result = {0};
    ipma_monitor_error_t monitor_err = ipma_monitor_fetch_today(&result);
    if (monitor_err != IPMA_MONITOR_OK) {
        ESP_LOGE(TAG, "IPMA fetch failed: %s", ipma_monitor_err_to_name(monitor_err));
        return;
    }

    ESP_LOGI(TAG,
             "Forecast date=%s run date=%s file date=%s DICO=%s lat=%.4f lon=%.4f rcm=%u (%s)",
             result.data_prev,
             result.data_run,
             result.file_date,
             result.dico,
             result.latitude,
             result.longitude,
             result.rcm_code,
             result.rcm_label);
}
