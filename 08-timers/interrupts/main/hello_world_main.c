#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

SemaphoreHandle_t button_semaphore;

void buttonIsPressed(void *args)
{
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(button_semaphore, &higherPriorityTaskWoken);

  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void app_main() {

  button_semaphore = xSemaphoreCreateBinary();

  if (button_semaphore == NULL) {
    printf("De semaphore kan niet gemaakt worden\n");
    return;
  }

  gpio_config_t button_config = {
    .pin_bit_mask = 1 << 4,
                      .mode = GPIO_MODE_INPUT,
                      .pull_up_en = GPIO_PULLUP_ENABLE,
                      .pull_down_en = GPIO_PULLDOWN_DISABLE,
                      .intr_type = GPIO_INTR_NEGEDGE
  };

  if (gpio_config(&button_config) != ESP_OK) {
    printf("Fout bij configureren GPIO\n");
    return;
  }

  if (gpio_install_isr_service(0) != ESP_OK) {
    printf("Fout bij installeren ISR-service\n");
    return;
  }

  if (gpio_isr_handler_add(GPIO_NUM_4, buttonIsPressed, NULL) != ESP_OK) {
    printf("Fout bij toevoegen ISR-handler\n");
    return;
  }


  while (1) {
    xSemaphoreTake(button_semaphore, portMAX_DELAY);
        printf("Button event ontvangen\n");

  }
}