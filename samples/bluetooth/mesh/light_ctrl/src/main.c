/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic mesh light fixture sample
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/dfu/mcuboot.h>
#include "model_handler.h"
#include "lc_pwm_led.h"

#ifdef CONFIG_MCUMGR_SMP_BT
#include <mgmt/mcumgr/smp_bt.h>
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include "os_mgmt/os_mgmt.h"
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include "img_mgmt/img_mgmt.h"
#endif
#include <device.h>
#include <soc.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG)
#define LOG_MODULE_NAME main_c
#include "common/log.h"

#ifdef CONFIG_EMDS
#include <emds/emds.h>

#define EMDS_DEV_IRQ 24
#define EMDS_DEV_PRIO 0
#define EMDS_ISR_ARG 0
#define EMDS_IRQ_FLAGS 0




#define BT_LE_ADV_SMP_PARAM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | \
					    BT_LE_ADV_OPT_USE_NAME, \
					    BT_GAP_ADV_SLOW_INT_MIN, \
					    BT_GAP_ADV_SLOW_INT_MAX, NULL)

static struct bt_conn *current_conn;
static bool stateConnected = false;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
		      0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

bool isConnected() {
    return stateConnected;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", addr);

	current_conn = bt_conn_ref(conn);

	// gp_led_on();
	stateConnected = true;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
		// gp_led_off();
	}
	stateConnected = false;
}

static struct bt_conn_cb conn_callbacks = {
	.connected    = connected,
	.disconnected = disconnected,
};

static void ble_hdl_init(void)
{
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
	os_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
	img_mgmt_register_group();
#endif
	bt_conn_cb_register(&conn_callbacks);
#ifdef CONFIG_MCUMGR_SMP_BT
	smp_bt_register();
#endif
}

static void ble_hdl_start(void)
{
	int rc = bt_le_adv_start(BT_LE_ADV_SMP_PARAM, ad, ARRAY_SIZE(ad), NULL, 0);
	if (rc) {
		LOG_ERR("Advertising SMP failed to start (code %d)", rc);
	} else {
		LOG_DBG("Advertising SMP successfully started");
	}
	LOG_INF("Application started\n");
}

static void button_handler_cb(uint32_t pressed, uint32_t changed)
{
	if (!bt_mesh_is_provisioned()) {
		return;
	}

	if (pressed & changed & BIT(3)) {
		NVIC_SetPendingIRQ(EMDS_DEV_IRQ);
	}
}

static void app_emds_cb(void)
{
	printk("SAMPLE HALTED!!!\n");
	dk_set_leds(DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK);
	k_fatal_halt(K_ERR_CPU_EXCEPTION);
}

static void isr_emds_cb(void *arg)
{
	ARG_UNUSED(arg);

#if defined(CONFIG_BT_CTLR)
	/* Stop mpsl to reduce power usage. */
	irq_disable(TIMER0_IRQn);
	irq_disable(RTC0_IRQn);
	irq_disable(RADIO_IRQn);

	mpsl_uninit();
#endif

	emds_store();
}
#endif

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	dk_leds_init();
	dk_buttons_init(NULL);

#ifdef CONFIG_EMDS
	static struct button_handler button_handler = {
		.cb = button_handler_cb,
	};

	dk_button_handler_add(&button_handler);

	err = emds_init(&app_emds_cb);
	if (err) {
		printk("Initializing emds failed (err %d)\n", err);
		return;
	}
#endif

	err = bt_mesh_init(bt_mesh_dk_prov_init(), model_handler_init());
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

#ifdef CONFIG_EMDS
	err = emds_load();
	if (err) {
		printk("Restore of emds data failed (err %d)\n", err);
		return;
	}

	err = emds_prepare();
	if (err) {
		printk("Preparation emds failed (err %d)\n", err);
		return;
	}

	IRQ_CONNECT(EMDS_DEV_IRQ, EMDS_DEV_PRIO, isr_emds_cb,
		    EMDS_ISR_ARG, EMDS_IRQ_FLAGS);
	irq_enable(EMDS_DEV_IRQ);
#endif

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	printk("Mesh initialized\n");

	model_handler_start();
}

void main(void)
{
	int err;

	printk("Initializing...\n");
	lc_pwm_led_init();
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	ble_hdl_init();
	ble_hdl_start();

	// err = boot_write_img_confirmed();
	// if (err) {
	// 	printk("Failed to confirm image: %d\n", err);
	// }
}
