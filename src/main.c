/*
 * OpenThread Sensor Node
 * Sleepy End Device
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor/sht4x.h>
#include <stdio.h>

#include <openthread/thread.h>
#include <openthread/udp.h>

#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>

#define DEBUG
#ifdef DEBUG
	#include <zephyr/drivers/uart.h>
	#include <zephyr/usb/usb_device.h>
	#include <zephyr/logging/log.h>
	LOG_MODULE_REGISTER(cdc_acm_echo, LOG_LEVEL_INF);
#endif

// the devicetree node identifier for the "led1_green" alias
#define LED1_GREEN_NODE DT_ALIAS(led1_green)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED1_GREEN_NODE, gpios);

#if DT_NODE_HAS_STATUS(LED1_GREEN_NODE, okay)
#define LED1_GREEN_PIN DT_GPIO_PIN(LED1_GREEN_NODE, gpios)
#endif

// the devicetree node identifier for our self-defined "pwr" alias.
#define PWR_IO_NODE DT_ALIAS(pwr)
static const struct gpio_dt_spec pwr = GPIO_DT_SPEC_GET(PWR_IO_NODE, gpios);

#if DT_NODE_HAS_STATUS(PWR_IO_NODE, okay)
	#define PWR_IO_PIN DT_GPIO_PIN(PWR_IO_NODE, gpios)
#endif

#define SLEEP_TIME 10

//-----------------------------
void udp_send(char *buf)
//-----------------------------
{
	otError ot_error = OT_ERROR_NONE;
	
	// fetch OpenThread instance and udp_socket
	otInstance *ot_instance;
	ot_instance = openthread_get_default_instance();
	otUdpSocket udp_socket;

	// prepare the message, target address and target port
	otMessageInfo message_info;
	memset(&message_info, 0, sizeof(message_info));
	// ff03::1 is the IPv6 broadcast address
	otIp6AddressFromString("ff03::1", &message_info.mPeerAddr);
	message_info.mPeerPort = 1234;

	// send the message
	do {
		ot_error = otUdpOpen(ot_instance, &udp_socket, NULL, NULL);
		if (ot_error != OT_ERROR_NONE) { 
			#ifdef DEBUG
				LOG_ERR("could not open udp port");
			#endif
			break; 
		}

		otMessage *json_message = otUdpNewMessage(ot_instance, NULL);
		ot_error = otMessageAppend(json_message, buf, (uint16_t)strlen(buf));
		if (ot_error != OT_ERROR_NONE) { 
			#ifdef DEBUG
				LOG_ERR("could not append message");
			#endif
			break; 
		}

		ot_error = otUdpSend(ot_instance, &udp_socket, json_message, &message_info);
		if (ot_error != OT_ERROR_NONE) { 
			#ifdef DEBUG
				LOG_ERR("could not send udp message");
			#endif
			break; 
		}

		ot_error = otUdpClose(ot_instance, &udp_socket);
	} while(false);

	#ifdef DEBUG
		if (ot_error == OT_ERROR_NONE) { 
			LOG_INF("udp message sent");
		}
	#endif
}

//-----------------------------
void main(void)
//-----------------------------
{
	char json_buf[100];
	int err;

	uint8_t eui64[8];
	char eui64_id[17];
	char buf[3];

	#ifdef DEBUG
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_HIGH);
		err = gpio_pin_set_dt(&led, 1);
		k_sleep(K_MSEC(500));

		const struct device *usbdev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

		if (usb_enable(NULL)) {
			LOG_ERR("Failed to enable USB");
			return;
		}

		err = gpio_pin_set_dt(&led, 0);
		LOG_INF("--- ot-sensor-sed ---\n");
	#endif

	// fetch OpenThread instance and EUI64
	otInstance *ot_instance;
	ot_instance = openthread_get_default_instance();
	otPlatRadioGetIeeeEui64(ot_instance, eui64);
	// convert EUI64 to hex string
	for (int i=0; i < 8; i++) {
		snprintf(buf, sizeof(buf),"%0X",eui64[i]);
		strcat(eui64_id,buf);
	}

	//-----------------------------
	// init SHT40 sensor
	//-----------------------------
	//err = gpio_pin_configure_dt(&pwr, GPIO_OUTPUT_HIGH|GPIO_OPEN_SOURCE);
	err = gpio_pin_configure_dt(&pwr, GPIO_OUTPUT_HIGH);
	err = gpio_pin_set_dt(&pwr, 1);

	const struct device *const sht = DEVICE_DT_GET_ANY(sensirion_sht4x);
	k_sleep(K_MSEC(500));
	
	if (!device_is_ready(sht)) {
		#ifdef DEBUG
			LOG_ERR("Device %s is not ready.\n", sht->name);
		#endif
		return;
	}
	#ifdef DEBUG
		LOG_INF("SHT40 Sensor started");
	#endif

	//-----------------------------
	// main loop
	//-----------------------------
	while (true) {

		//------------------------------------
		// take measurement
		//------------------------------------
		struct sensor_value temp, humidity;

		if (sensor_sample_fetch(sht)) {
			#ifdef DEBUG
				LOG_ERR("Failed to fetch sample from SHT4X device\n");
			#endif
			return;
		}
		sensor_channel_get(sht, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(sht, SENSOR_CHAN_HUMIDITY, &humidity);

		//------------------------------------
		// construct json message
		//------------------------------------
		snprintf(json_buf, sizeof(json_buf),
			"{ \"id\": \"%s\", \"temp\": %d.%d, \"hum\": %d.%d }",
			eui64_id, temp.val1, temp.val2, humidity.val1, humidity.val2);

		#ifdef DEBUG
			LOG_INF("JSON message: %s",json_buf);
		#endif

		//------------------------------------
		// broadcast udp message
		//------------------------------------
		udp_send(json_buf);

		// clear json buffer
		json_buf[0] = "\0";

		//------------------------------------
		// go to sleep
		//------------------------------------
		// suspend BME280 sensor saves ca. 400µA
		err = pm_device_action_run(sht, PM_DEVICE_ACTION_SUSPEND);
		k_sleep(K_SECONDS(SLEEP_TIME));
		err = pm_device_action_run(sht, PM_DEVICE_ACTION_RESUME);
	}
}
