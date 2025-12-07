/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

LOG_MODULE_REGISTER(time_demo, LOG_LEVEL_INF);

/* Button assignments */
#define BUTTON_AUTHORITY	DK_BTN1_MSK
#define BUTTON_CLIENT		DK_BTN2_MSK
#define BUTTON_RELAY		DK_BTN3_MSK
#define BUTTON_RESET		DK_BTN4_MSK

/* LED assignments */
#define LED_AUTHORITY		DK_LED1
#define LED_CLIENT		DK_LED2
#define LED_RELAY		DK_LED3
#define LED_PROVISIONED		DK_LED4

/* Periodic time printing interval (5 seconds) */
#define TIME_PRINT_INTERVAL	K_SECONDS(5)

static struct k_work_delayable time_print_work;


/* Update LEDs based on current role */
void update_role_leds(void)
{
	enum bt_mesh_time_role role = model_handler_role_get();

	dk_set_led_off(LED_AUTHORITY);
	dk_set_led_off(LED_CLIENT);
	dk_set_led_off(LED_RELAY);

	switch (role) {
	case BT_MESH_TIME_AUTHORITY:
		dk_set_led_on(LED_AUTHORITY);
		LOG_INF("Role: Time Authority");
		break;
	case BT_MESH_TIME_CLIENT:
		dk_set_led_on(LED_CLIENT);
		LOG_INF("Role: Time Client");
		break;
	case BT_MESH_TIME_RELAY:
		dk_set_led_on(LED_RELAY);
		LOG_INF("Role: Time Relay");
		break;
	default:
		LOG_INF("Role: None");
		break;
	}
}

/* Periodic work to print current time */
static void time_print_work_handler(struct k_work *work)
{
	model_handler_time_print();

	/* Reschedule */
	k_work_schedule(&time_print_work, TIME_PRINT_INTERVAL);
}

/* Button handler */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	static uint64_t reset_press_time;

	if (has_changed & BUTTON_AUTHORITY) {
		if (button_state & BUTTON_AUTHORITY) {
			model_handler_role_set(BT_MESH_TIME_AUTHORITY);
		}
	}

	if (has_changed & BUTTON_CLIENT) {
		if (button_state & BUTTON_CLIENT) {
			model_handler_role_set(BT_MESH_TIME_CLIENT);
		}
	}

	if (has_changed & BUTTON_RELAY) {
		if (button_state & BUTTON_RELAY) {
			model_handler_role_set(BT_MESH_TIME_RELAY);
		}
	}

	if (has_changed & BUTTON_RESET) {
		if (button_state & BUTTON_RESET) {
			/* Button pressed - record time */
			reset_press_time = k_uptime_get();
		} else {
			/* Button released - check duration */
			uint64_t press_duration = k_uptime_get() - reset_press_time;

			if (press_duration > 5000) {
				/* Long press - factory reset */
				LOG_INF("Factory reset requested");
				bt_mesh_reset();
				dk_set_led_off(LED_PROVISIONED);
			}
		}
	}
}

int main(void)
{
	int err;

	LOG_INF("Bluetooth Mesh Time Demo");

	/* Initialize Bluetooth */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	/* Initialize hardware */
	err = dk_leds_init();
	if (err) {
		LOG_ERR("LED init failed (err %d)", err);
		return err;
	}

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Button init failed (err %d)", err);
		return err;
	}

	/* Initialize model handler */
	err = model_handler_init();
	if (err) {
		LOG_ERR("Model handler init failed (err %d)", err);
		return err;
	}

	/* Initialize Bluetooth Mesh */
	err = bt_mesh_init(bt_mesh_dk_prov_init(), model_handler_comp_data_get());
	if (err) {
		LOG_ERR("Bluetooth Mesh init failed (err %d)", err);
		return err;
	}

	/* Load stored settings */
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
		LOG_INF("Settings loaded");
	}

	/* Set default time if not already set (e.g., from settings) */
	model_handler_set_default_time_if_needed();

	/* Check if already provisioned */
	if (bt_mesh_is_provisioned()) {
		LOG_INF("Mesh network restored from flash");
		dk_set_led_on(LED_PROVISIONED);
	} else {
		/* Enable provisioning */
		err = bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
		if (err) {
			LOG_ERR("Provisioning enable failed (err %d)", err);
			return err;
		}
		LOG_INF("Provisioning enabled");
	}

	/* Initialize periodic time printing work */
	k_work_init_delayable(&time_print_work, time_print_work_handler);
	k_work_schedule(&time_print_work, TIME_PRINT_INTERVAL);

	LOG_INF("Initialization complete");
	LOG_INF("Press Button 1 for Time Authority role");
	LOG_INF("Press Button 2 for Time Client role");
	LOG_INF("Press Button 3 for Time Relay role");
	LOG_INF("Press and hold Button 4 (5s) for factory reset");

	return 0;
}
