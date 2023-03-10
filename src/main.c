/*
 * thread_cli demo program
 */
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <openthread/thread.h>
#include <openthread/udp.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>

// #define DEBUG
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

const char device_id[] = "D9:28:1F:ED:79:2A";

/*
 * Get a device structure from a devicetree node with compatible
 * "bosch,bme280". (If there are multiple, just pick one.)
 */
static const struct device *get_bme280_device(void)
{
	const struct device *const dev = DEVICE_DT_GET_ANY(bosch_bme280);

	if (dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		#ifdef DEBUG 
			LOG_ERR("\nError: no device found.\n");
		#endif
		return NULL;
	}

	if (!device_is_ready(dev)) {
		#ifdef DEBUG 
			LOG_ERR("\nError: Device \"%s\" is not ready; "
		       		"check the driver initialization logs for errors.\n",
		       		dev->name);
		#endif
		return NULL;
	}

	#ifdef DEBUG 
		LOG_INF("Found device \"%s\", getting sensor data\n", dev->name);
	#endif
	return dev;
}

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

	#ifdef DEBUG
		const struct device *usbdev;
		uint32_t baudrate, dtr = 0U;

		usbdev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
		if (!device_is_ready(usbdev)) {
			LOG_ERR("CDC ACM device not ready");
			return;
		}

		ret = usb_enable(NULL);
		if (ret != 0) {
			LOG_ERR("Failed to enable USB");
			return;
		}
	#endif

	//-----------------------------
	// init sensor
	//-----------------------------
	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_HIGH);
	err = gpio_pin_toggle_dt(&led);
	k_sleep(K_SECONDS(1));

	// BME280 sensor
	const struct device *dev = get_bme280_device();
	if (dev == NULL) {
		return;
	}
	err = gpio_pin_toggle_dt(&led);
	#ifdef DEBUG
		LOG_INF("BME280 Sensor started");
	#endif

	//-----------------------------
	// main loop
	//-----------------------------
	while (true) {

		//------------------------------------
		// take measurement
		//------------------------------------
		struct sensor_value temp, press, humidity;

		sensor_sample_fetch(dev);
		sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press);
		sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);

		#ifdef DEBUG
			LOG_INF("temp: %d.%d; press: %d.%d; humidity: %d.%d",
					temp.val1, temp.val2, press.val1, press.val2,
					humidity.val1, humidity.val2);
		#endif

		//------------------------------------
		// construct json message
		//------------------------------------
		snprintf(json_buf, sizeof(json_buf),
			"{ \"id\": \"%s\", \"temp\": %d.%d, \"press\": %d.%d, \"hum\": %d.%d }",
			device_id, temp.val1, temp.val2, press.val1, press.val2, humidity.val1, humidity.val2);

		#ifdef DEBUG
			LOG_INF("JSON message built");
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
		k_sleep(K_SECONDS(10));
	}
}
