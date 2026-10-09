#include <stdio.h>
#include "esp_timer.h"
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct
{
    int64_t  start_time;
    uint32_t duration;
    bool running;

    uint32_t expected;
    uint32_t fired;
} Timer;


Timer timer;

void timer_callback(void *arg)
{
    Timer *timer = (Timer *)arg;
    timer->fired++;

    int64_t elapsed = esp_timer_get_time() / 1000 - timer->start_time;
    timer->expected = elapsed / 1000;

    printf("Timer %" PRIu32 " (rooster: %" PRIu32 ") fired at %" PRId64 "ms!\n",
       timer->fired,
       timer->expected,
       esp_timer_get_time() / 1000);

    if (timer->fired <= 3)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void)
{
    const esp_timer_create_args_t oneshot_timer_args = {
        .callback = &timer_callback,
        .name = "one-shot",
        .arg = &timer,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true};

    esp_timer_handle_t oneshot_timer = NULL;
    timer.start_time = esp_timer_get_time() / 1000;

    esp_err_t ret = esp_timer_create(&oneshot_timer_args, &oneshot_timer);

    if (ret != ESP_OK)
    {
        printf("Timer creation failed\n");
    }

    esp_timer_start_periodic(oneshot_timer, 1000000);
}
