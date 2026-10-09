#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi_default.h"

const char *reason_to_text(int reason)
{
    switch (reason)
    {
    case WIFI_REASON_NO_AP_FOUND:
        return "netwerk niet gevonden";

    case WIFI_REASON_AUTH_FAIL:
        return "authenticatie mislukt (wachtwoord?)";

    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "4-way handshake timeout";

    default:
        return "onbekende reden";
    }
}

void wifi_event_handler(
    void *event_handler_arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{

    printf("event_id = %ld\n", event_id);

    if (event_id == WIFI_EVENT_STA_START)
    {
        printf("STA gestart, ik ga verbinding maken\n");

        esp_err_t result_connect = esp_wifi_connect();

        if (result_connect != ESP_OK)
        {
            printf("Verbinding maken mislukt: %s\n",
                   esp_err_to_name(result_connect));
        }
    }

    if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        printf("de wifi-event wordt nu uitgevoerd\n");
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        printf("Verbinding verbroken: %s\n",
               reason_to_text(((wifi_event_sta_disconnected_t *)event_data)->reason));
    }

    if (event_id == IP_EVENT_STA_GOT_IP ) {

         printf("in IP if\n");

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        printf("cast gelukt\n");

        printf("IP adres: " IPSTR "\n", IP2STR(&event->ip_info.ip));

    }
}

void app_main(void)
{
    esp_err_t result_nvs = nvs_flash_init();

    if (result_nvs != ESP_OK)
    {
        printf("NVS initialisatie mislukt: %s\n",
               esp_err_to_name(result_nvs));
    }

    esp_err_t result_event = esp_event_loop_create_default();

    if (result_event != ESP_OK)
    {
        printf("de event loop kan niet gemaakt worden: %s\n",
               esp_err_to_name(result_event));
        return;
    }

    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_err_t result_init = esp_wifi_init(&cfg);

    if (result_init != ESP_OK)
    {
        printf("WiFi initialisatie mislukt: %s\n",
               esp_err_to_name(result_init));
        return;
    }

    esp_err_t result_mode = esp_wifi_set_mode(WIFI_MODE_STA);

    if (result_mode != ESP_OK)
    {
        printf("WiFi mode instellen mislukt: %s\n",
               esp_err_to_name(result_mode));
        return;
    }

    wifi_config_t wifi_config = {};

    const char *ssid = "Ziggo1056914";

    if (strlen(ssid) > sizeof(wifi_config.sta.ssid))
    {
        printf("de ssid is te groot\n");
        return;
    }

    memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));

    const char *password = "nf9Trqzhfmft";

    if (strlen(password) > sizeof(wifi_config.sta.password))
    {
        printf("het wachtwoord is te groot\n");
        return;
    }

    memcpy(wifi_config.sta.password, password, strlen(password));

    esp_err_t result_wifi_config =
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    if (result_wifi_config != ESP_OK)
    {
        printf("Er is iets mis met de WiFi-configuratie: %s\n",
               esp_err_to_name(result_wifi_config));
        return;
    }

    esp_err_t result_handler = esp_event_handler_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_CONNECTED,
        wifi_event_handler,
        NULL);

    if (result_handler != ESP_OK)
    {
        printf("registreren van CONNECTED handler mislukt: %s\n",
               esp_err_to_name(result_handler));
        return;
    }

    result_handler = esp_event_handler_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        wifi_event_handler,
        NULL);

    if (result_handler != ESP_OK)
    {
        printf("registreren van DISCONNECTED handler mislukt: %s\n",
               esp_err_to_name(result_handler));
        return;
    }

    result_handler = esp_event_handler_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_START,
        wifi_event_handler,
        NULL);

     result_handler = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL);

    if (result_handler != ESP_OK)
    {
        printf("registreren van START handler mislukt: %s\n",
               esp_err_to_name(result_handler));
        return;
    }

    esp_err_t result_start = esp_wifi_start();

    if (result_start != ESP_OK)
    {
        printf("WiFi starten is mislukt: %s\n",
               esp_err_to_name(result_start));
        return;
    }
}