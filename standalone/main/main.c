/*
 * CAM Monitor H.264 standalone application.
 * No ESP-KVM startup, login, USB HID, recorder, MQTT, VPN or MJPEG.
 * SPDX-License-Identifier: MIT
 */
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "capture.h"
#include "video_frame.h"
#include "kvm_caps.h"
#include "kvm_settings.h"
#include "kvm_thermal.h"
#include "kvm_webrtc.h"
#include "wifi_ap.h"
#include "web.h"

static const char *TAG = "cammonitor";

void app_main(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);
    ESP_ERROR_CHECK(kvm_settings_init()); /* driver settings, no UI wizard */
    kvm_caps_init();
    kvm_thermal_init(); /* thermal protection for H.264 chip */

    video_frame_store_init();

    /* Do encoder allocations first while contiguous internal RAM is available. */
    capture_reserve_early();

    ESP_ERROR_CHECK(cam_wifi_start_open_ap());
    ESP_ERROR_CHECK(cam_web_start());
    if (kvm_webrtc_init(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "WebRTC not started: a video client cannot connect");
    }
    capture_start();
    ESP_LOGI(TAG, "Open WiFi CAM-MONITOR and http://192.168.4.1");
}
