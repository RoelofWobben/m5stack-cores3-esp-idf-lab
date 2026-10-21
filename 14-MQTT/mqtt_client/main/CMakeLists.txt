idf_component_register(SRCS "wifi.c" "hello_world_main.c" "mqtt.c"
                       PRIV_REQUIRES spi_flash nvs_flash esp_event esp_netif esp_wifi mqtt
                       INCLUDE_DIRS "")
