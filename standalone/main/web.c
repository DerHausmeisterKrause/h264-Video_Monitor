/*
 * Single-page HTTP server. A H.264-only signaling endpoint will be introduced
 * with the camera capture pipeline; no JPEG endpoint or fallback is compiled.
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "web.h"

static const char *TAG = "cam_web";
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, (size_t)(index_html_end - index_html_start));
}

static esp_err_t status_get(httpd_req_t *req)
{
    // truthful until H.264 encoder + RTP/WebRTC wiring is implemented.
    static const char body[] =
        "{\"app\":\"CAM Monitor\",\"transport\":\"webrtc\","
        "\"codec\":\"h264\",\"videoReady\":false,"
        "\"note\":\"HDMI-CSI/H.264 integration pending\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

esp_err_t cam_web_start(void)
{
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.max_uri_handlers = 4;
    conf.stack_size = 6144;
    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    const httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_get};
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root), TAG, "root handler");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &status), TAG, "status handler");
    return ESP_OK;
}
