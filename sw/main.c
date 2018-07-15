#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"

// below which light intensity should the lamp be turned on
#define LIGHT_TRESHOLD 400

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	return ESP_OK;
}

void app_main(void)
{
	nvs_flash_init();
	tcpip_adapter_init();

	tcpip_adapter_ip_info_t ipInfo;
	IP4_ADDR(&ipInfo.ip, 10,0,0,1);
	IP4_ADDR(&ipInfo.gw, 10,0,0,1);
	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo);

	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
	wifi_config_t sta_config = {
		.ap = {
			.ssid = "ilt3t",
			.password = "knowledge",
			.channel = 1,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK,
			.ssid_hidden = 0,
			.max_connection = 2
		}
	};
	ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &sta_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );

	// set gpios as output
	// internal led
	gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
	// external pin which drives lamp via mosfet
	gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);

	// onfigre adc
	adc1_config_width(ADC_WIDTH_12Bit);
	adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_0db);

	// offset for when the lamp is on
	int offset_own_light = 0;

	// current status of lamp
	bool lamp_on = false;

	// main loop (wifi is handled in different thread/processor)
	while (true)
	{
		// should lamp be on currently
		bool turn_on = false;

		// read adc value (ldr light intensity)
		int current_adc = (adc1_get_voltage(ADC1_CHANNEL_0));

		// if light level low enough, turn lamp on
		if(current_adc < (LIGHT_TRESHOLD + offset_own_light))
		{
			turn_on = true;
		}

		printf("1 t: %d - l: %d - a: %d - o: %d\n", turn_on, lamp_on, current_adc, offset_own_light);
		if(turn_on && !lamp_on)
		{
			// update internal state
			lamp_on = true;
			// turn on internal led
			gpio_set_level(GPIO_NUM_5, 1);
			// turn on mosfet driver for lamp
			gpio_set_level(GPIO_NUM_17, 1);
			// sleep to let adc adjust - dont sleep too long or
			// values might change too much
			vTaskDelay(50 / portTICK_PERIOD_MS);
			// measure again to get current offset
			int current_adc2 = (adc1_get_voltage(ADC1_CHANNEL_0));
			// ignore intensity added by own lamp
			offset_own_light = current_adc2 - current_adc;
			// keep it on at least for some time
		//	vTaskDelay(70 / portTICK_PERIOD_MS);
		}
		else if(!turn_on && lamp_on)
		{
			// update internal state
			lamp_on = false;
			// turn off internal led
			gpio_set_level(GPIO_NUM_5, 0);
			// turn off mosfet driver
			gpio_set_level(GPIO_NUM_17, 0);
			// reset offset since lamp is now off
			offset_own_light = 0;
		}
		if(1)
		{
			// add some delay to reduce power consumption
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}

		printf("2 t: %d - l: %d - a: %d - o: %d\n", turn_on, lamp_on, current_adc, offset_own_light);
	}
}

