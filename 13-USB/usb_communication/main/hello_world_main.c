
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "driver/usb_serial_jtag.h"

#define BUF_SIZE 128
#define MAX_MESSAGE_LENGTH 64

static const char *TAG = "USB_RX";
static QueueHandle_t display_queue;

void send_task(void *pvParameters)
{
    while (1)
    {
        printf("Hallo vanaf de M5!\n");
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("test 1 2 3\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void display_task(void *pvParameters)
{
    char message[MAX_MESSAGE_LENGTH + 1];

    while (1)
    {
        BaseType_t result = xQueueReceive(
            display_queue,
            message,
            portMAX_DELAY
        );

        if (result == pdPASS)
        {
            printf("Display: %s\n", message);
        }
    }
}

void app_main(void)
{
    display_queue = xQueueCreate(10, MAX_MESSAGE_LENGTH + 1);

    if (display_queue == NULL)
    {
        ESP_LOGE(TAG, "Aanmaken display_queue mislukt");
        return;
    }

    usb_serial_jtag_driver_config_t usb_config = {
        .rx_buffer_size = BUF_SIZE,
        .tx_buffer_size = BUF_SIZE,
    };

    ESP_ERROR_CHECK(
        usb_serial_jtag_driver_install(&usb_config)
    );

    BaseType_t result = xTaskCreate(
        send_task,
        "send_task",
        2048,
        NULL,
        10,
        NULL
    );

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Aanmaken send_task mislukt");
        return;
    }

    result = xTaskCreate(
        display_task,
        "display_task",
        2048,
        NULL,
        10,
        NULL
    );

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Aanmaken display_task mislukt");
        return;
    }

    uint8_t data[BUF_SIZE];
    char message[MAX_MESSAGE_LENGTH + 1];
    size_t message_len = 0;

    message[0] = '\0';

    ESP_LOGI(TAG, "USB ontvangen gestart. Wacht op gegevens...");

    while (1)
    {
        int len = usb_serial_jtag_read_bytes(
            data,
            sizeof(data),
            20 / portTICK_PERIOD_MS
        );

        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                if (data[i] == '\r')
                {
                    message[message_len] = '\0';

                    BaseType_t queue_result = xQueueSend(
                        display_queue,
                        message,
                        0
                    );

                    if (queue_result != pdPASS)
                    {
                        ESP_LOGE(
                            TAG,
                            "Versturen naar display_queue mislukt"
                        );
                    }

                    message_len = 0;
                    message[0] = '\0';
                }
                else if (message_len >= MAX_MESSAGE_LENGTH)
                {
                    ESP_LOGE(TAG, "Message is te groot!");

                    message_len = 0;
                    message[0] = '\0';
                }
                else
                {
                    message[message_len] = (char)data[i];
                    message_len++;
                }
            }
        }
    }
}
