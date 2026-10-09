#include <stdio.h>
#include "esp_timer.h"

void timer_callback(void *arg)
{
    printf("Timer fired!\n");
}

void timer_periodic_callback(void *arg)
{
    printf("periodic Timer fired!\n");
}


void app_main(void)
{
    const esp_timer_create_args_t oneshot_timer_args = {
        .callback = &timer_callback,
        .name = "one-shot",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = false};

    esp_timer_handle_t oneshot_timer = NULL;

    esp_err_t ret = esp_timer_create(&oneshot_timer_args, &oneshot_timer);

    if (ret != ESP_OK)
    {
        printf("Timer creation failed\n");
    }

    esp_timer_start_once(oneshot_timer, 2000000);


    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &timer_periodic_callback,
        .name = "periodic",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = false};

    esp_timer_handle_t periodic_timer = NULL;

    ret = esp_timer_create(&periodic_timer_args, &periodic_timer);

    if (ret != ESP_OK)
    {
        printf("Timer creation failed\n");
    }

    esp_timer_start_periodic(periodic_timer, 2000000);
}
