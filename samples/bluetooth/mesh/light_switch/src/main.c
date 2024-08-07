/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic mesh light switch sample
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

static uint16_t subnets[CONFIG_BT_MESH_SUBNET_COUNT];
static uint16_t cnt;
int status;

static void node_id_set(enum bt_mesh_feat_state node_id_ip)
{
	enum bt_mesh_feat_state node_id;

	for (uint8_t i = 0; i < cnt; i++) {
		status = bt_mesh_subnet_node_id_set(subnets[i], node_id_ip);
		printf("node_id_set Status %d Subnet %d Node ID %d\n", status, subnets[i], node_id_ip);

		status = bt_mesh_subnet_node_id_get(subnets[i], &node_id);
		printk("node_id_get Status %d Subnet %d Node ID %d\n", status, subnets[i], node_id);
	}
}

static void node_id_get()
{
	enum bt_mesh_feat_state node_id;

	for (uint8_t i = 0; i < cnt; i++) {
		status = bt_mesh_subnet_node_id_get(subnets[i], &node_id);
		printf("node_id_get Status %d Subnet %d Node ID %d\n", status, subnets[i], node_id);
	}
}

static void lpn_established(uint16_t net_idx, uint16_t friend_addr,
			    uint8_t queue_size, uint8_t recv_window)
{
	printk("LPN established with Friend 0x%04x\n", friend_addr);
	bt_mesh_gatt_proxy_set(BT_MESH_FEATURE_DISABLED);

	/* Disable node ID and print status of node ID advs */
	node_id_set(BT_MESH_FEATURE_DISABLED);
}

static void lpn_terminated(uint16_t net_idx, uint16_t friend_addr)
{
	printk("LPN lost with Friend 0x%04x\n", friend_addr);
	bt_mesh_gatt_proxy_set(BT_MESH_FEATURE_ENABLED);

	/* Enable node ID and print status of node ID advs */
	node_id_set(BT_MESH_FEATURE_ENABLED);
}

BT_MESH_LPN_CB_DEFINE(lpn_cb) = {
	.established = lpn_established,
	.terminated = lpn_terminated,
};


static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = dk_leds_init();
	if (err) {
		printk("Initializing LEDs failed (err %d)\n", err);
		return;
	}

	err = dk_buttons_init(NULL);
	if (err) {
		printk("Initializing buttons failed (err %d)\n", err);
		return;
	}

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


	k_sleep(K_SECONDS(10));

	if (IS_ENABLED(CONFIG_BT_MESH_LOW_POWER)) {
		bt_mesh_lpn_set(true);
	}

	cnt = bt_mesh_subnets_get(subnets, ARRAY_SIZE(subnets), 0);
	if (cnt) {
		printk("Got %u subnet%s\n", cnt, cnt == 1 ? "" : "s");
		node_id_get();
	} else {
		printk("Got no subnets\n");
	}

}

int main(void)
{
	int err;

	printk("Initializing...\n");

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	return 0;
}
