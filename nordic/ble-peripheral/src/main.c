/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <dk_buttons_and_leds.h>

// Device Name
#define DEVICE_NAME     "BLE-LED"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// LEDs
#define RUN_STATUS_LED          DK_LED1
#define CON_STATUS_LED          DK_LED2
#define RUN_LED_BLINK_INTERVAL  1000

// User LED
#define USER_LED                DK_LED3

// Store the current value of the User LED
static uint8_t led_value = 0;

/** @brief LBS Service UUID. */
#define BT_UUID_SIMPLE_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x65210001, 0x28D5, 0x4B7B, 0xBADF, 0x7DEE1E8D1B6D)

// Service:        Simple Service UUID      65210001-28D5-4B7B-BADF-7DEE1E8D1B6D
static struct bt_uuid_128 simple_service_uuid = 
    BT_UUID_INIT_128(0x6d, 0x1b, 0x8d, 0x1e, 0xee, 0x7d, 0xdf, 0xba, 0x7b,
             0x4b, 0xd5, 0x28, 0x01, 0x00, 0x21, 0x65);

// Characteristic: LED1 Characteristic UUID 65210002-28D5-4B7B-BADF-7DEE1E8D1B6D
static struct bt_uuid_128 led1_char_uuid = 
    BT_UUID_INIT_128(0x6d, 0x1b, 0x8d, 0x1e, 0xee, 0x7d, 0xdf, 0xba, 0x7b,
             0x4b, 0xd5, 0x28, 0x02, 0x00, 0x21, 0x65);

// Advertising Data
static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SIMPLE_SERVICE_VAL)
};

// LED1 read value function
static ssize_t read_led(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{

    printk("Read value of User LED (DK LED 3): 0x%02x\n", led_value);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &led_value, sizeof(led_value));
}

void led_change_state(uint8_t new_state)
{
    printk("Received value 0x%02x. Turning LED %s\n", new_state, (new_state? "ON":"OFF"));

	dk_set_led(DK_LED3, new_state);
}

// LED1 write value function
static ssize_t write_led(struct bt_conn *conn,
               const struct bt_gatt_attr *attr, const void *buf,
               uint16_t len, uint16_t offset, uint8_t flags)
{
    printk("Received value for LED1: 0x%02x\n", ((uint8_t *) buf)[0]);

    uint8_t value = ((uint8_t *) buf)[0];

    // Change the state of LED1
    led_change_state(value);
	led_value = value;

    return len;
}

// Instantiate the service and its characteristics
BT_GATT_SERVICE_DEFINE(
    simple_service,
    
    // Simple Service
    BT_GATT_PRIMARY_SERVICE(&simple_service_uuid),

    // LED1 Characteristic
    // Properties: Read, Write
    BT_GATT_CHARACTERISTIC(&led1_char_uuid.uuid,
                    BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT,
                    read_led,
                    write_led,
                    &led_value)
);

// Connected state callback
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}

	printk("Connected\n");

	dk_set_led_on(CON_STATUS_LED);
}

// Disconnected state callback
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);

	dk_set_led_off(CON_STATUS_LED);
}

// Register BLE connection callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
};

void main(void)
{
	int blink_status = 0;
	int err;

	printk("BLE Peripheral Project! Built on the %s\n", CONFIG_BOARD);

	err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return;
	}
	
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}	
}
