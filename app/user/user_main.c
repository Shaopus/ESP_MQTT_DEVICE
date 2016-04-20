/*
 *  MQTT Device
 *
 *  This project is meant to control a relay in e.g. a power outlet or power strip and
 *  upload temperature and humidity through MQTT.
 *
 *  The ESP8266 will register itself with the MQTT server and will listen to topic
 *  /DeviceX/<chip-ID>. Inbound message are expected to be formatted as JSON messages
 *  and will be parsed for switching instruction. Please find a valid JSON instruction
 *  below:
 *
 *  {"switch":"off"}
 *  or
 *  {"switch":"on"}
 *
 *
 *  The relay is supposed to be connected to ESP Pin GPIO12
 *  To experiment with the project, a LED will of course also do.
 *
 *  Optionally a push button can be connected meant to override messages from the
 *  MQTT broker, allowing you to physically switch the relay as well.
 *  When the push button is pressed, the relay will change its state and a JSON
 *  message is sent to the MQTT server indicating its new state.
 *  The optional push button should be connected to ESP Pin GPIO0 and when the
 *  button is pressed, this pin should be grounded.
 *
 *  (c) 2016 by ShaoPu <shaopus@163.com>
 *
 */
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "../mqtt/include/mqtt.h"
#include "../modules/include/wifi.h"
#include "../modules/include/config.h"
#include "../mqtt/include/debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "user_json.h"

MQTT_Client mqttClient;

#if ENVSENSOR_DEVICE

LOCAL os_timer_t dhtTimer;
LOCAL char temp[10];
LOCAL char hum[10];

#endif

#if PLUG_DEVICE

LOCAL int ICACHE_FLASH_ATTR
plug_switch_on() {
	INFO("BUTTON: Switch on\r\n");
	GPIO_OUTPUT_SET(SWITCH_GPIO, SWITCH_INVERT ? 0 : 1);
}

LOCAL int ICACHE_FLASH_ATTR
plug_switch_off() {
	INFO("BUTTON: Switch off\r\n");
	GPIO_OUTPUT_SET(SWITCH_GPIO, SWITCH_INVERT ? 1 : 0);
}

LOCAL unsigned char ICACHE_FLASH_ATTR
plug_switch_state() {
	return GPIO_INPUT_GET(SWITCH_GPIO);
}

LOCAL int ICACHE_FLASH_ATTR
plug_json_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    if (os_strncmp(path, "switch", 6) == 0) {
    	if (SWITCH_INVERT) {
    		jsontree_write_string(js_ctx, plug_switch_state() ? "off" : "on");
    	} else {
    		jsontree_write_string(js_ctx, plug_switch_state() ? "on" : "off");
    	}
    }
    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
plug_json_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            char buffer[64];
            os_bzero(buffer, 64);
            if (jsonparse_strcmp_value(parser, "switch") == 0) {
                jsonparse_next(parser);
                jsonparse_next(parser);
                jsonparse_copy_value(parser, buffer, sizeof(buffer));
                if (!strcoll(buffer, "on")) {
                	plug_switch_on();
                } else if (!strcoll(buffer, "off")) {
                	plug_switch_off();
                }
            }
        }
    }
    return 0;
}

LOCAL struct jsontree_callback plug_switch_callback =
    JSONTREE_CALLBACK(plug_json_get, plug_json_set);
JSONTREE_OBJECT(plug_switch_tree,
                JSONTREE_PAIR("switch", &plug_switch_callback));
JSONTREE_OBJECT(plug_device_tree,
				JSONTREE_PAIR("device", &plug_switch_tree));
#endif

#if ENVSENSOR_DEVICE
LOCAL int ICACHE_FLASH_ATTR
envsensor_json_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    if (os_strncmp(path, "temperature", 11) == 0) {
        jsontree_write_string(js_ctx, temp);
    } else if (os_strncmp(path, "humidity", 8) == 0) {
        jsontree_write_string(js_ctx, hum);
    }
    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
envsensor_json_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    return 0;
}

LOCAL struct jsontree_callback envsensor_switch_callback =
    JSONTREE_CALLBACK(envsensor_json_get, envsensor_json_set);
JSONTREE_OBJECT(envsensor_switch_tree,
                JSONTREE_PAIR("temperature", &envsensor_switch_callback),
				JSONTREE_PAIR("humidity", &envsensor_switch_callback));
JSONTREE_OBJECT(envsensor_device_tree,
				JSONTREE_PAIR("device", &envsensor_switch_tree));
#endif

#if PLUG_DEVICE

void ICACHE_FLASH_ATTR
plug_send_switch_state() {
	char *json_buf = NULL;
	json_buf = (char *)os_zalloc(jsonSize);
	json_ws_send((struct jsontree_value *)&plug_device_tree, "device", json_buf);
	INFO("BUTTON: Sending current switch status\r\n");
	MQTT_Publish(&mqttClient, sysCfg.mqtt_topic, json_buf, strlen(json_buf), 0, 0);
	os_free(json_buf);
	json_buf = NULL;
}

#endif


void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}
void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");
#if PLUG_DEVICE

	MQTT_Subscribe(client, sysCfg.mqtt_topic, 0);
	plug_send_switch_state();

#endif

}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	if (!strcoll(topicBuf, sysCfg.mqtt_topic)) {
		struct jsontree_context js;
#if PLUG_DEVICE
		jsontree_setup(&js, (struct jsontree_value *)&plug_device_tree, json_putchar);
#endif
		json_parse(&js, dataBuf);
	}
	os_free(topicBuf);
	os_free(dataBuf);
}

#if PLUG_DEVICE

void ICACHE_FLASH_ATTR
plug_button_press(void) {
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts

	// Button interrupt received
	INFO("BUTTON: Button pressed\r\n");

	// Button pressed, flip switch
//	if (GPIO_REG_READ(BUTTON_GPIO) & BIT12) {
	if (plug_switch_state()) {
		plug_switch_off();
	} else  {
		plug_switch_on();
	}

	// Send new status to the MQTT broker
	plug_send_switch_state();

	// Debounce
	os_delay_us(200000);

	// Clear interrupt status
	uint32 gpio_status;
	gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void ICACHE_FLASH_ATTR
plug_gpio_init(void) {
	// Configure switch (relay)
	PIN_FUNC_SELECT(SWITCH_GPIO_MUX, SWITCH_GPIO_FUNC);
	GPIO_OUTPUT_SET(SWITCH_GPIO, 0);

	// Configure push button
	if (BUTTON_CONNECTED) {
		PIN_FUNC_SELECT(BUTTON_GPIO_MUX, BUTTON_GPIO_FUNC); // Set function
		GPIO_DIS_OUTPUT(GPIO_ID_PIN(BUTTON_GPIO)); // Set as input
		ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
		ETS_GPIO_INTR_ATTACH(plug_button_press, NULL);  // GPIO0 interrupt handler
		gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_GPIO), GPIO_PIN_INTR_NEGEDGE); // Interrupt on negative edge
		ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
	}
}
#endif

#if ENVSENSOR_DEVICE
LOCAL void ICACHE_FLASH_ATTR dhtCb(void *arg)
{
	static char data[256];
	uint8_t status;
	os_timer_disarm(&dhtTimer);
	struct dht_sensor_data* r = DHTRead();
	float curTemp = r->temperature;
	float curHum = r->humidity;
	static float lastTemp;
	static float lastHum;
	uint8_t topic[32];
	if(r->success)
	{
		os_sprintf(temp, "%d.%d",(int)(curTemp),(int)((curTemp - (int)curTemp)*100));
		os_sprintf(hum, "%d.%d",(int)(curHum),(int)((curHum - (int)curHum)*100));
		INFO("SENSOR: Temperature: %s *C, Humidity: %s %%\r\n", temp, hum);
		if ((mqttClient.connState == MQTT_DATA && lastTemp != curTemp) ||
			(mqttClient.connState == MQTT_DATA && lastHum != curHum)) {

			// Send new status to the MQTT broker
			char *json_buf = NULL;
			json_buf = (char *)os_zalloc(jsonSize);
			json_ws_send((struct jsontree_value *)&envsensor_device_tree, "device", json_buf);
			INFO("BUTTON: Sending temperature and humidity\r\n");
			MQTT_Publish(&mqttClient, config.mqtt_topic, json_buf, strlen(json_buf), 0, 0);
			os_free(json_buf);
			json_buf = NULL;

			lastTemp = curTemp;
			lastHum = curHum;
		}
	}
	else
	{
		INFO("Error reading temperature and humidity.\r\n");
	}
	os_timer_setfn(&dhtTimer, (os_timer_func_t *)dhtCb, (void *)0);
	os_timer_arm(&dhtTimer, DHT_DELAY, 1);
}
#endif

void ICACHE_FLASH_ATTR
mqtt_init(void) {
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	//MQTT_InitConnection(&mqttClient, "192.168.0.164", 1883, 0);

	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	//MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
}

void user_rf_pre_init(void)
{
}

void user_init(void)
{
	uart_init(115200, 115200);
	INFO("\r\nSDK version: %s\n", system_get_sdk_version());
	INFO("System init...\r\n");
//	os_delay_us(1000000);

	CFG_Load();
#if PLUG_DEVICE
	plug_gpio_init();
	plug_switch_off();
#endif

#if ENVSENSOR_DEVICE
	DHTInit(DHT22);
#endif
	mqtt_init();

	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

#if ENVSENSOR_DEVICE
	os_timer_disarm(&dhtTimer);
	os_timer_setfn(&dhtTimer, (os_timer_func_t *)dhtCb, (void *)0);
	os_timer_arm(&dhtTimer, DHT_DELAY, 1);
#endif
	INFO("\r\nSystem started ...\r\n");
}
