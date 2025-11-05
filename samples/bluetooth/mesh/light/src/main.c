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
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt_callbacks.h>
#include <zephyr/mgmt/mcumgr/grp/settings_mgmt/settings_mgmt_callbacks.h>

#if IS_ENABLED(CONFIG_MCUMGR_TRANSPORT_BT)
static atomic_t dfu_erase_requested;

static enum mgmt_cb_return dfu_mgmt_cb(uint32_t event, enum mgmt_cb_return prev_status,
					  int32_t *rc, uint16_t *group, bool *abort_more,
					  void *data, size_t data_size)
{
	ARG_UNUSED(prev_status);
	ARG_UNUSED(rc);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);

	switch (event) {
	case MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK: {
		struct img_mgmt_upload_check *check = (struct img_mgmt_upload_check *)data;
		// if (check && check->action && check->action->erase) {
	if (check && check->req && check->req->upgrade == true) {
			printk("DFU upgrade requested: setting flag\n");
			atomic_set(&dfu_erase_requested, 1);
		}
		break;
	}
	case MGMT_EVT_OP_IMG_MGMT_DFU_PENDING: {
		printk("DFU pending\n");
		if (atomic_get(&dfu_erase_requested)) {
			printk("DFU erase requested: resetting Mesh state\n");
			bt_mesh_reset();
			atomic_clear(&dfu_erase_requested);
		}
		break;
	}
	case MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED: {
		printk("DFU confirmed\n");
		struct img_mgmt_image_confirmed *confirmed = (struct img_mgmt_image_confirmed *)data;
		printk("DFU confirmed: image %d\n", confirmed->image);
		bt_mesh_reset();
		break;
	}
	default:
		break;
	}

	return MGMT_CB_OK;
}

static enum mgmt_cb_return settings_mgmt_cb(uint32_t event, enum mgmt_cb_return prev_status,
					    int32_t *rc, uint16_t *group, bool *abort_more,
					    void *data, size_t data_size)
{
	ARG_UNUSED(prev_status);
	ARG_UNUSED(rc);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);

	printk("Settings event: %d\n", event);
	switch (event) {
	case MGMT_EVT_OP_SETTINGS_MGMT_ACCESS: {
		struct settings_mgmt_access *access = (struct settings_mgmt_access *)data;
		if (access && access->access == SETTINGS_ACCESS_DELETE) {
			/* Reset mesh state when settings are being deleted */
			printk("Settings erase requested: resetting Mesh state\n");
			bt_mesh_reset();
		}
		break;
	}
	default:
		break;
	}

	return MGMT_CB_OK;
}

static struct mgmt_callback dfu_mgmt_cb_handle = {
	.callback = dfu_mgmt_cb,
	.event_id = (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK | MGMT_EVT_OP_IMG_MGMT_DFU_PENDING | MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED),
};

static struct mgmt_callback settings_mgmt_cb_handle = {
	.callback = settings_mgmt_cb,
	.event_id = MGMT_EVT_OP_SETTINGS_MGMT_ACCESS,
};
#endif

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

		/* Register DFU erase handling via MCUmgr callbacks */
		mgmt_callback_register(&dfu_mgmt_cb_handle);

		/* Register settings erase handling via MCUmgr callbacks */
		mgmt_callback_register(&settings_mgmt_cb_handle);
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
