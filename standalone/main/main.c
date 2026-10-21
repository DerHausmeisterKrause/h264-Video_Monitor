/*
 * CAM Monitor / standalone ESP32-P4-NANO (3.x)
 * Minimal bootstrap. Video pipeline deliberately not claimed ready yet.
 * SPDX-License-Identifier: MIT
 */
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "web.h"

static const char *TAG = "cammonitor";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Standalone firmware: no login, no setup, no MJPEG");
    ESP_LOGI(TAG, "Only H.264 will be allowed once capture is integrated");

    ESP_ERROR_CHECK(cam_wifi_start_open_ap());
    ESP_ERROR_CHECK(cam_web_start());
    ESP_LOGI(TAG, "Connect to CAM-MONITOR, then visit http://192.168.4.1/");
}
