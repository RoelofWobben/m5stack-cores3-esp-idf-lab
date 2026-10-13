#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include <sys/socket.h>
#include "esp_wifi_default.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <errno.h>

static bool tcp_task_started = false;
static const char *server_ip = "192.168.178.21";

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

void tcp_client_task(void *pvParameters);

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
            printf("Disconnect reason code: %d\n", ((wifi_event_sta_disconnected_t *)event_data)->reason);

        printf("Verbinding verbroken: %s\n",
               reason_to_text(((wifi_event_sta_disconnected_t *)event_data)->reason));
    }

    if (event_id == IP_EVENT_STA_GOT_IP)
    {

        printf("in IP if\n");

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        printf("cast gelukt\n");

        printf("IP adres: " IPSTR "\n", IP2STR(&event->ip_info.ip));

        if (!tcp_task_started)
        {
            BaseType_t result = xTaskCreate(
                tcp_client_task,
                "tcp_client",
                4096,
                NULL,
                5,
                NULL);

            if (result == pdPASS)
            {
                tcp_task_started = true;
                printf("TCP-task aangemaakt\n");
            }
            else
            {
                printf("TCP-task aanmaken mislukt\n");
            }
        }
    }
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

bool controleer_invoer(char *input, float *waarde)
{
    char *pos_delimiter = strchr(input, ':');

    if (pos_delimiter == NULL)
    {
        return false;
    }

    if ((pos_delimiter - input) != 4)
    {
        return false;
    }

    char part1[5];
    strncpy(part1, input, 4);
    part1[4] = '\0';

    if (strcmp("TEMP", part1) != 0)
    {
        return false;
    }

    size_t lengte = strlen(pos_delimiter + 1);
    printf("Lengte van temperatuurtekst: %zu\n", lengte);
    printf("Waarde na dubbele punt: '%s'\n", pos_delimiter + 1);

    if (lengte == 0 || lengte > 4)
    {
        return false;
    }

    char part2[5];
    strncpy(part2, pos_delimiter + 1, lengte);
    part2[lengte] = '\0';

    char *endptr;
    float resultaat = strtof(part2, &endptr);

    if (endptr == part2 || *endptr != '\0')
    {
        return false;
    }

    *waarde = resultaat;

    return true;
}

void tcp_client_task(void *pvParameters)
{
    printf("TCP-task gestart\n");
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        printf("Socket aanmaken mislukt\n");
        vTaskDelete(NULL);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(5000),
    };

    int result_ip = inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    printf("result_ip = %d\n", result_ip);

    if (result_ip <= 0)
    {
        printf("Het IP-adres kon niet worden omgezet\n");
        close(sock);
        return;
    }

    int result_connect = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (result_connect < 0)
    {
        printf("Verbinding maken met de server mislukt\n");
        printf("errno = %d\n", errno);
        close(sock);
        vTaskDelete(NULL);
    }

    printf("Verbinding met de server gelukt\n");

    const char *message = "Hallo, desktop van de M5!";
    ssize_t bytes_sent = send(sock, message, strlen(message), 0);

    if (bytes_sent == -1)
    {
        printf("Versturen van het bericht mislukt\n");
    }

    if (bytes_sent != strlen(message))
    {
        printf("Niet alle bytes zijn verstuurd\n");
    }

    // ontvangen van een bericht van de server

    char buff[100];
    ssize_t result_ontvangen = recv(sock, buff, 99, 0);

    if (result_ontvangen > 0)
    {
        buff[result_ontvangen] = '\0';
        printf("Ontvangen boodschap: %s\n", buff);

        float temperatuur;
        buff[strcspn(buff, "\n")] = '\0';

        if (controleer_invoer(buff, &temperatuur))
        {
            printf("Temperatuur: %.2f\n", temperatuur);
        }
        else
        {
            printf("Illegale invoer\n");
        }
    }
    else if (result_ontvangen == 0)
    {
        printf("De peer heeft de TCP-verbinding gesloten.\n");
    }
    else
    {
        printf("recv mislukt, errno = %d\n", errno);
    }

    vTaskDelete(NULL);
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

    const char *ssid = "assortimens";

    if (strlen(ssid) > sizeof(wifi_config.sta.ssid))
    {
        printf("de ssid is te groot\n");
        return;
    }

    memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));

    const char *password = "t3mpp4ssw0rd";

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