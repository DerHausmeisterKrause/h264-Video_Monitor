/*
 * H.264-only CSI capture loop.
 * Adapted from ESP-KVM capture_loop.c: Apache-2.0, ESP-KVM contributors (2026).
 * Unlike upstream, NO MJPEG codec switching or fallback.
 */
#include "capture_priv.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "kvm_settings.h"
#include "kvm_thermal.h"
#include "video_frame.h"

void capture_loop_run(capture_ctx_t *c)
{
    video_frame_store_init();
    const capture_codec_t *codec = capture_codec_h264();
    esp_err_t err = codec->open();
    int64_t last_encode_us = 0;
    int64_t recover_after = 0;
    bool first = true;

    for (;;) {
        esp_task_wdt_reset();
        if (err != ESP_OK) {
            ESP_LOGW(CAPTURE_LOG_TAG, "H.264 unavailable (%s), retry in 2s; no MJPEG fallback",
                     esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_task_wdt_reset();
            err = codec->open();
            first = true;
            continue;
        }
        if (c->mode_change_pending) {
            const uint32_t w = c->pending_hres, h = c->pending_vres;
            c->mode_change_pending = false;
            if (capture_hw_apply_mode(c, w, h) != ESP_OK) {
                ESP_LOGE(CAPTURE_LOG_TAG, "CSI mode change to %ux%u failed", w, h);
            }
            first = true;
            video_frame_request_keyframe();
            continue;
        }
        if (xSemaphoreTake(c->csi_done_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
            if (c->signal_present && !c->mode_too_fast &&
                esp_timer_get_time() >= recover_after) {
                (void)capture_hw_hdmi_recover(c);
                recover_after = esp_timer_get_time() + 8000000;
            }
            continue;
        }
        recover_after = 0;
        capture_hw_frames_flowing();
        while (xSemaphoreTake(c->csi_done_sem, 0) == pdTRUE) {}
        capture_hw_frame_gate_open();
        if (c->ready_fb_idx < 0 || video_frame_viewer_count() == 0) continue;

        const int fps = kvm_thermal_fps_limit((int)kvm_setting_int("vid_fps_max"));
        if (fps == 0) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (fps > 0) {
            const int64_t now = esp_timer_get_time();
            if (now - last_encode_us < 1000000 / fps) continue;
            last_encode_us = now;
        }

        portENTER_CRITICAL(&c->fb_lock);
        const int idx = c->ready_fb_idx;
        c->held_fb_idx = idx;
        portEXIT_CRITICAL(&c->fb_lock);
        if (idx < 0) continue;
        if (!capture_park_frame_begin()) {
            portENTER_CRITICAL(&c->fb_lock);
            c->held_fb_idx = -1;
            portEXIT_CRITICAL(&c->fb_lock);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        void *src = c->fb[idx];
        const int64_t arrived = capture_hw_fb_landed_us(idx);
        video_frame_set_source_time(arrived);
        if (arrived > 0) capture_status_add_wait((uint32_t)(esp_timer_get_time() - arrived));
        (void)esp_cache_msync(src, c->frame_bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        const esp_err_t encoded = codec->encode(c, src, first);
        if (encoded == ESP_OK) {
            first = false;
        } else if (encoded == ESP_ERR_NO_MEM || capture_h264_encoder_failed()) {
            ESP_LOGE(CAPTURE_LOG_TAG, "H.264 encoder failed (%s), retry; no MJPEG",
                     esp_err_to_name(encoded));
            (void)video_frame_quiesce(5000);
            codec->close();
            err = encoded;
        }
        capture_park_frame_end();
        portENTER_CRITICAL(&c->fb_lock);
        c->held_fb_idx = -1;
        portEXIT_CRITICAL(&c->fb_lock);
    }
}
