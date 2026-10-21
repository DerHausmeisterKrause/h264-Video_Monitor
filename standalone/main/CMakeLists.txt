idf_component_register(
    SRCS "main.c" "wifi_ap.c" "web.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_event
        esp_netif
        esp_wifi
        esp_http_server
        nvs_flash
        esp_driver_gpio
        espressif__esp_hosted
        espressif__esp_wifi_remote
    EMBED_FILES "../web/index.html"
)
