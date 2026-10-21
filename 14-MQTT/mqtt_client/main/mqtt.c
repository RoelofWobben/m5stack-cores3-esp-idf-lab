#include <stdio.h>

#include "mqtt.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "esp_err.h"

static esp_mqtt_client_handle_t client;

const char *mqtt_connect_error_to_text(esp_mqtt_connect_return_code_t code)
{
    if (code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED)
    {
        return "Not authorized: check username and password";
    }
    else
    {
        return "Other MQTT connection error";
    }
}

// MQTT event handler
void mqtt_event_handler(void *event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void *event_data)
{
    if (event_id == MQTT_EVENT_CONNECTED)
    {
        printf("MQTT connected!\n");

        int result = esp_mqtt_client_publish(
            client,
            "greenhouse/test",
            "Hallo laptop!",
            0,
            0,
            0);

        printf("MQTT publish result: %d\n", result);

        int result_subscribe = esp_mqtt_client_subscribe(client, "greenhouse/command", 0);
        printf("MQTT subscribe result: %d\n", result_subscribe);
    }
    else if (event_id == MQTT_EVENT_DATA)
    {
        esp_mqtt_event_t *mqtt_event = (esp_mqtt_event_t *)event_data;
        printf("MQTT TOPIC: %.*s\n", mqtt_event->topic_len, mqtt_event->topic);
        printf("%.*s\n", mqtt_event->data_len, mqtt_event->data);
    }
    else if (event_id == MQTT_EVENT_DISCONNECTED)
    {
        printf("MQTT disconnected!\n");
    }
    else if (event_id == MQTT_EVENT_ERROR)
    {
        esp_mqtt_event_t *mqtt_event = (esp_mqtt_event_t *)event_data;
        esp_mqtt_error_codes_t *errorcode = mqtt_event->error_handle;

        if (errorcode != NULL)
        {

            if (errorcode->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
            {
                printf("MQTT connection error: %s\n",
                       mqtt_connect_error_to_text(errorcode->connect_return_code));
            }
        }

        else
        {
            printf("MQTT error information is NULL\n");
        }
    }
    else
    {
        printf("MQTT event received: %ld\n", event_id);
    }
}

// Initialize MQTT client
void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {};

    mqtt_cfg.broker.address.uri = "XXXXX";
    mqtt_cfg.credentials.username = "xxxxxx";
    mqtt_cfg.credentials.client_id = "XXXXX";
    mqtt_cfg.credentials.authentication.password = "XXXXXX";

    // Initialize MQTT client
    client = esp_mqtt_client_init(&mqtt_cfg);

    if (client == NULL)
    {
        printf("Failed to initialize MQTT client\n");
        return;
    }

    // Register MQTT event handler
    esp_err_t result_event_handler = esp_mqtt_client_register_event(
        client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL);

    if (result_event_handler != ESP_OK)
    {
        printf("Failed to register MQTT event handler: %s\n",
               esp_err_to_name(result_event_handler));
        return;
    }

    // Start MQTT client
    esp_err_t result_client = esp_mqtt_client_start(client);

    if (result_client != ESP_OK)
    {
        printf("Failed to start MQTT client: %s\n",
               esp_err_to_name(result_client));
        return;
    }
}