#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include "freertos/queue.h"
#include "string.h"
#include "sdkconfig.h"
#include "freertos/event_groups.h"

#define LED_GPIO GPIO_NUM_18
#define BUTTON_GPIO GPIO_NUM_4
#define BUF 1024

xQueueHandle xQueue;

EventGroupHandle_t task_watchdog;
const uint32_t task_event_group_id = (1 << 0);
const uint32_t tasks_all_bits = task_event_group_id;


void isr_button_pressed(void *args) {

	char mydata[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	int btn_state = gpio_get_level(BUTTON_GPIO);
	gpio_set_level(LED_GPIO,btn_state);

	xQueueSendFromISR(xQueue,(void *)&mydata[0],(TickType_t )0); // add the counter value to the queue
}

void task_event_group( void *pvParameter ) {
  while (1) {
	  xEventGroupSetBits(task_watchdog, task_event_group_id);

	  vTaskDelay(1000);
  }
  vTaskDelete( NULL );
}

void task_sw_watchdog( void *pvParameter ) {

	while (1) {
		uint32_t result = xEventGroupWaitBits (task_watchdog, tasks_all_bits, pdTRUE, pdTRUE, 2000);

		if (result  == tasks_all_bits) {
			printf("System is healthy!");
		}
		else {
			if (!(result & task_event_group_id)) {
				printf ("Task stopped responding!");
			}
		}
	}


}

void task_messenger(void *pvParameter) {
		char rxmesage[27];

	    while(1) {
	    	if( xQueue != 0 ) {

				if( (xQueueReceiveFromISR( xQueue,  &rxmesage[0], 500/( portTICK_PERIOD_MS ) )) == pdTRUE) {

					printf("value received on queue: %s \n",&rxmesage[0]);

				}
				vTaskDelay(1500/portTICK_PERIOD_MS); //wait for 500 ms
	    	}
	    }
}

void task_button_to_led(void *pvParameter) {
 	//Configure button
	gpio_config_t btn_config;
	btn_config.intr_type = GPIO_INTR_HIGH_LEVEL;
	btn_config.mode = GPIO_MODE_INPUT;        	//Set as Input
	btn_config.pin_bit_mask = (1 << BUTTON_GPIO); //Bitmask
	btn_config.pull_up_en = GPIO_PULLUP_DISABLE; 	//Disable pullup
	btn_config.pull_down_en = GPIO_PULLDOWN_ENABLE; //Enable pulldown
	gpio_config(&btn_config);
	printf("Button configured\n");

	//Configure LED
	gpio_pad_select_gpio(LED_GPIO);					//Set pin as GPIO
	gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);	//Set as Output
  	printf("LED configured\n");

  	//Configure interrupt and add handler
  	gpio_install_isr_service(0);						//Start Interrupt Service Routine service
  	gpio_isr_handler_add(BUTTON_GPIO, isr_button_pressed, NULL); //Add handler of interrupt
  	printf("Interrupt configured\n");

  	//Wait
  	while (1) {
  		gpio_set_level(GPIO_NUM_2,1);
  		vTaskDelay(500/portTICK_PERIOD_MS);
  		gpio_set_level(GPIO_NUM_2,0);
  		vTaskDelay(500/portTICK_PERIOD_MS);
  	}
}

void app_main() {
	xQueue = xQueueCreate( 10, 27);
	task_watchdog = xEventGroupCreate();

	xTaskCreate(&task_button_to_led, "buttonToLED", 2048, NULL, 1, NULL);
	xTaskCreate(&task_messenger, "task_messenger", 2048, NULL, 1, NULL);

	xTaskCreate(&task_event_group, "task_event_group", 2048, NULL, 1, NULL);
	xTaskCreate(&task_sw_watchdog, "task_sw_watchdog", 2048, NULL, 2, NULL);
}
