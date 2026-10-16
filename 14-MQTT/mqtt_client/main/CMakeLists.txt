idf_component_register(SRCS "wifi.c" "hello_world_main.c"
                       PRIV_REQUIRES spi_flash nvs_flash esp_event esp_netif esp_wifi
                       INCLUDE_DIRS "")
