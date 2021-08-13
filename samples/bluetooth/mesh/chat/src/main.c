/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic Mesh light sample
 */
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(chat, CONFIG_LOG_DEFAULT_LEVEL);

static uint8_t dev_key[16] = {1};
static uint8_t net_key[16] = {2};
static uint8_t app_key[16] = {3};

extern struct bt_mesh_model models_vnd[];

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	dk_leds_init();
	dk_buttons_init(NULL);

	err = bt_mesh_init(bt_mesh_dk_prov_init(), model_handler_init());
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	printk("Mesh initialized\n");

	err = bt_mesh_provision(net_key, 0, 0, 0, CONFIG_DEV_UNICAST_ADDR, dev_key);
	if (err != -EALREADY && err != 0) {
		printk("Error in self provisioning (err %d)\n", err);
	}

	err = bt_mesh_app_key_add(0, 0, app_key);
	if (err) {
		printk("AppKey add failed (err %d)", err);
		return;
	}
	
	/* Models must be bound to an app key to send and receive messages with
	 * it:
	 */
	models_vnd[0].keys[0] = 0;	
}

void main(void)
{
	int err;

	printk("Initializing...\n");
	k_sleep(K_MSEC(10000));
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
}
