/*
 * Minimal ESP32-C6 SDIO Wi-Fi access point. Not an ESP-KVM service.
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "wifi_ap.h"

static const char *TAG = "cam_wifi";

esp_err_t cam_wifi_start_open_ap(void)
{
    // Internal SDIO pull-ups supplement the module's external resistors.
    const int sdio_data_and_cmd[] = {14, 15, 16, 17, 19};
    for (size_t i = 0; i < sizeof(sdio_data_and_cmd) / sizeof(sdio_data_and_cmd[0]); ++i) {
        gpio_pullup_en((gpio_num_t)sdio_data_and_cmd[i]);
    }

    ESP_LOGI(TAG, "Starting ESP32-C6 hosted SDIO link");
    int hosted_err = esp_hosted_init();
    if (hosted_err != 0) {
        ESP_LOGE(TAG, "esp_hosted_init failed: %d", hosted_err);
        return ESP_FAIL;
    }
    hosted_err = esp_hosted_connect_to_slave();
    if (hosted_err != 0) {
        ESP_LOGE(TAG, "esp_hosted_connect_to_slave failed: %d", hosted_err);
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init");
    esp_err_t event = esp_event_loop_create_default();
    if (event != ESP_OK && event != ESP_ERR_INVALID_STATE) {
        return event;
    }
    esp_netif_t *ap = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(ap != NULL, ESP_ERR_NO_MEM, TAG, "create AP interface");

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "esp_wifi_init");

    wifi_config_t config = {0};
    strncpy((char *)config.ap.ssid, "CAM-MONITOR", sizeof(config.ap.ssid) - 1);
    config.ap.ssid_len = strlen("CAM-MONITOR");
    config.ap.channel = 6;
    config.ap.max_connection = 1;
    config.ap.authmode = WIFI_AUTH_OPEN; // User-requested passwordless hotspot.

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "ap mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "ap config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "ap start");
    ESP_LOGW(TAG, "OPEN hotspot CAM-MONITOR (no password); video will be visible to nearby users");
    return ESP_OK;
}

/*
 * ESP-KVM contributors, Apache-2.0: move esp-hosted DMA allocations to PSRAM
 * to preserve internal RAM for the H.264 encoder. Standalone implementation.
 */
void *__real_eh_host_port_dma_alloc(size_t size);
void *__wrap_eh_host_port_dma_alloc(size_t size)
{
    const size_t line = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
    void *p = heap_caps_malloc((size + line - 1) & ~(line - 1),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
    return p ? p : __real_eh_host_port_dma_alloc(size);
}
