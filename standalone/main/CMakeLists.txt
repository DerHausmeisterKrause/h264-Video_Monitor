idf_component_register(
    SRCS "main.c" "wifi_ap.c" "web.c"
    INCLUDE_DIRS "."
    REQUIRES
        video_pipeline
        kvm_webrtc
        kvm_config
        kvm_board
        esp_event
        esp_netif
        esp_wifi
        esp_http_server
        nvs_flash
        esp_driver_gpio
        espressif__cjson
        espressif__esp_hosted
        espressif__esp_wifi_remote
    EMBED_FILES "../web/index.html"
)
