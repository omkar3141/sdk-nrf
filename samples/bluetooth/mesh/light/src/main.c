/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic mesh light sample
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

static const uint8_t default_key[16] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
};

#define ADDR_LIGHT_SW 	(0x0010)
#define ADDR_LIGHT 		(0x0020)

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

	if (bt_mesh_is_provisioned()) {
		goto skip_self_prov;
	}

	uint16_t addr = ADDR_LIGHT;
	uint8_t status;

	err = bt_mesh_provision(default_key, 0, 0, 0, addr, default_key);
	if (err) {
		printk("Failed to provision. err: %d\n", err);
		return;
	}

	err = bt_mesh_cfg_app_key_add(0, addr, 0, 0, default_key, &status);
	if (err || status) {
		printk("Failed to add app key. err: %d\n", err);
		return;
	}

	err = bt_mesh_cfg_mod_app_bind(0, addr, addr, 0,
				BT_MESH_MODEL_ID_GEN_ONOFF_SRV, &status);
	if (err) {
		printk("Failed to bind model. err: %d\n", err);
		return;
	}

	printk("light self provisioned. addr: 0x%04x\n", addr);

skip_self_prov:

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	printk("Mesh initialized\n");
}

void main(void)
{
	int err;

	printk("Initializing...\n");

	err = bt_enable(NULL);
	bt_ready(0);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
}
