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
#include "smp_bt.h"

/* Set maximum possible data that can be sent over extended advertising. Subtract "2"
 * to account for AD Lenth and AD type. */
#define MANUFACTURER_DATA_LEN (CONFIG_BT_CTLR_ADV_DATA_LEN_MAX - 2)

static struct bt_le_ext_adv *adv;
static uint8_t manufacturer_data[MANUFACTURER_DATA_LEN];

static struct bt_le_adv_param ext_adv_param = {
        .id = BT_ID_DEFAULT,
        .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_NAME,
        .interval_min = BT_GAP_ADV_SLOW_INT_MIN,
        .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
        .peer = NULL,
};

static bool rxadv_data_parser(struct bt_data *data, void *user_data)
{
	if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len > 29)  {
		/* If first character of manufacturer data is not "00", we reject it
		 * and don't print it */
		if (data->data[0] != 0x00) {
			return true;
		}

		printk("Received manufacturer data: len %02d: ", data->data_len);
		for (int i = 0; i < data->data_len - 1; i++) {
			printk("%02x", data->data[i]);
		}
		printk("\n");
	}

	return true;
}

static void scan_recv_cb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	bt_data_parse(buf, rxadv_data_parser, NULL);
	return;
}

static struct bt_le_scan_cb scan_cb = {
	.recv = scan_recv_cb,
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

	if (IS_ENABLED(CONFIG_MCUMGR_TRANSPORT_BT)) {
		err = smp_dfu_init();
		if (err) {
			printk("Unable to initialize DFU (err %d)\n", err);
		}
	}

	err = bt_le_ext_adv_create(&ext_adv_param, NULL, &adv);
	if (err) {
	    printk("Failed to create advertising set (err %d)\n", err);
	    return;
	}

	/* Some manufacturer data : 0001020304...*/
	for (int i = 0; i < MANUFACTURER_DATA_LEN; i++) {
		manufacturer_data[i] = i;
	}

	struct bt_data ad[] = {
		BT_DATA(BT_DATA_MANUFACTURER_DATA, manufacturer_data, MANUFACTURER_DATA_LEN),
	};

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
	    printk("Failed to set advertising data (err %d)\n", err);
	    return;
	}

	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
	    printk("Failed to start extended advertising (err %d)\n", err);
	    return;
	}

	printk("Extended advertising started\n");
	bt_le_scan_cb_register(&scan_cb);

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
