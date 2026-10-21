/*
 * Minimal H.264-only WebRTC signaling and settings interface.
 * No auth, no setup mode, no KVM control or JPEG endpoint. MIT.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "capture.h"
#include "kvm_caps.h"
#include "kvm_settings.h"
#include "kvm_webrtc.h"
#include "web.h"

#define OFFER_LIMIT 16384
#define BODY_LIMIT 1024
static const char *TAG = "cam_web";
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");
static uint32_t s_client_id = 0;

static esp_err_t json_reply(httpd_req_t *req, cJSON *root)
{
    if (!root) return ESP_ERR_NO_MEM;
    char *output = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!output) return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t result = httpd_resp_sendstr(req, output);
    free(output);
    return result;
}

static esp_err_t failure(httpd_req_t *req, const char *status, const char *msg)
{
    httpd_resp_set_status(req, status);
    cJSON *j = cJSON_CreateObject();
    if (!j) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(j, "error", msg);
    return json_reply(req, j);
}

static char *body_read(httpd_req_t *req, size_t limit)
{
    if (req->content_len < 1 || (size_t)req->content_len > limit) return NULL;
    char *buf = malloc((size_t)req->content_len + 1);
    if (!buf) return NULL;
    int seen = 0, stalls = 0;
    while (seen < req->content_len) {
        int n = httpd_req_recv(req, buf + seen, req->content_len - seen);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++stalls <= 3) continue;
        if (n <= 0) { free(buf); return NULL; }
        seen += n; stalls = 0;
    }
    buf[seen] = 0;
    return buf;
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, (size_t)(index_html_end - index_html_start));
}

static esp_err_t settings_get(httpd_req_t *req)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "h264_kbps", kvm_setting_int("h264_kbps"));
    cJSON_AddNumberToObject(j, "vid_fps_max", kvm_setting_int("vid_fps_max"));
    cJSON_AddNumberToObject(j, "edid_prof", kvm_setting_int("edid_prof"));
    return json_reply(req, j);
}

static esp_err_t settings_put(httpd_req_t *req)
{
    char *body = body_read(req, BODY_LIMIT);
    if (!body) return failure(req, "400 Bad Request", "Invalid settings request");
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!cJSON_IsObject(j)) {
        cJSON_Delete(j);
        return failure(req, "400 Bad Request", "JSON object required");
    }
    const struct {const char *key; int min; int max;} settings[] = {
        {"h264_kbps", 500, 20000}, {"vid_fps_max", 1, 60}, {"edid_prof", 0, 2}
    };
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        const cJSON *val = cJSON_GetObjectItemCaseSensitive(j, settings[i].key);
        if (val && (!cJSON_IsNumber(val) || val->valuedouble != val->valueint ||
                    val->valueint < settings[i].min || val->valueint > settings[i].max)) {
            cJSON_Delete(j);
            return failure(req, "400 Bad Request", "Setting outside allowed range");
        }
    }
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        const cJSON *val = cJSON_GetObjectItemCaseSensitive(j, settings[i].key);
        if (val && kvm_setting_set_int(settings[i].key, val->valueint) != ESP_OK) {
            cJSON_Delete(j);
            return failure(req, "500 Internal Server Error", "Cannot save setting");
        }
    }
    cJSON_Delete(j);
    return settings_get(req);
}

static esp_err_t status_get(httpd_req_t *req)
{
    kvm_video_status_t status = {0};
    capture_status_get(&status);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "codec", "h264");
    cJSON_AddBoolToObject(j, "signal", status.signal);
    cJSON_AddNumberToObject(j, "width", status.hres);
    cJSON_AddNumberToObject(j, "height", status.vres);
    cJSON_AddNumberToObject(j, "fps", (double)status.fps_x100 / 100.0);
    cJSON_AddNumberToObject(j, "kbps", status.kbps);
    cJSON_AddBoolToObject(j, "h264Ready", kvm_cap_available(KVM_CAP_H264));
    cJSON_AddBoolToObject(j, "webrtcReady", kvm_cap_available(KVM_CAP_WEBRTC));
    return json_reply(req, j);
}

static esp_err_t webrtc_post(httpd_req_t *req)
{
    char *body = body_read(req, OFFER_LIMIT);
    if (!body) return failure(req, "413 Payload Too Large", "Invalid WebRTC offer");
    cJSON *j = cJSON_Parse(body);
    free(body);
    const cJSON *sdp = cJSON_GetObjectItemCaseSensitive(j, "sdp");
    if (!cJSON_IsString(sdp)) {
        cJSON_Delete(j);
        return failure(req, "400 Bad Request", "Expected an SDP offer");
    }
    const uint8_t owner[32] = {0};
    char *answer = NULL, error[160] = {0};
    uint32_t new_id = 0;
    int http_status = 500;
    esp_err_t result = kvm_webrtc_offer(sdp->valuestring, owner, &answer,
                                        &new_id, &http_status, error, sizeof(error));
    cJSON_Delete(j);
    if (result != ESP_OK) {
        char status_line[40];
        snprintf(status_line, sizeof(status_line), "%d %s", http_status,
                 http_status == 503 ? "Service Unavailable" : "Bad Request");
        return failure(req, status_line, error[0] ? error : "WebRTC unavailable");
    }
    if (s_client_id && s_client_id != new_id) kvm_webrtc_close(s_client_id);
    s_client_id = new_id;
    cJSON *out = cJSON_CreateObject();
    cJSON_AddNumberToObject(out, "id", new_id);
    cJSON_AddStringToObject(out, "sdp", answer);
    free(answer);
    return json_reply(req, out);
}

static esp_err_t webrtc_delete(httpd_req_t *req)
{
    char query[40] = {0}, id[12] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK) {
        const uint32_t requested = (uint32_t)strtoul(id, NULL, 10);
        if (requested == s_client_id) {
            kvm_webrtc_close(requested);
            s_client_id = 0;
        }
    }
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t cam_web_start(void)
{
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.max_uri_handlers = 8;
    conf.stack_size = 8192;
    conf.max_open_sockets = 5;
    httpd_handle_t h = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&h, &conf), TAG, "httpd_start");
    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=root_get},
        {.uri="/api/status", .method=HTTP_GET, .handler=status_get},
        {.uri="/api/v1/settings", .method=HTTP_GET, .handler=settings_get},
        {.uri="/api/v1/settings", .method=HTTP_PUT, .handler=settings_put},
        {.uri="/api/v1/video/webrtc", .method=HTTP_POST, .handler=webrtc_post},
        {.uri="/api/v1/video/webrtc", .method=HTTP_DELETE, .handler=webrtc_delete},
    };
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(h, &routes[i]), TAG, "register route");
    }
    ESP_LOGI(TAG, "Live camera page on http://192.168.4.1/");
    return ESP_OK;
}
