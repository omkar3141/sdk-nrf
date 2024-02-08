/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/**
 * @file
 * @brief Model handler for the Silvair EnOcean sample.
 *
 * Instantiates a Silvair EnOcean Proxy Server model on the devkit, as well as
 * the standard Config and Health Server models. Handles all application
 * behavior related to the models.
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/enocean.h>
#include <bluetooth/mesh/vnd/silvair_enocean_srv.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include "lc_pwm_led.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(model_handler, LOG_LEVEL_INF);

static struct bt_mesh_dtt_srv dtt_srv = BT_MESH_DTT_SRV_INIT(NULL);

/* Silvair Enocean Proxy behavior */
static void onoff_status(struct bt_mesh_onoff_cli *cli,
			 struct bt_mesh_msg_ctx *ctx,
			 const struct bt_mesh_onoff_status *status);

static void level_status(struct bt_mesh_lvl_cli *cli,
			 struct bt_mesh_msg_ctx *ctx,
			 const struct bt_mesh_lvl_status *status);

struct bt_mesh_silvair_enocean_srv silvair_enocean = {
	.buttons[0 ... 1] = { .onoff = BT_MESH_ONOFF_CLI_INIT(&onoff_status),
			      .lvl = BT_MESH_LVL_CLI_INIT(&level_status), },
};

static void onoff_status(struct bt_mesh_onoff_cli *cli,
			 struct bt_mesh_msg_ctx *ctx,
			 const struct bt_mesh_onoff_status *status)
{
	struct bt_mesh_silvair_enocean_button *button =
		CONTAINER_OF(cli, struct bt_mesh_silvair_enocean_button, onoff);
	int index = button - &silvair_enocean.buttons[0];

	dk_set_led(index, status->present_on_off);

	printk("Button %d: Received onoff response: %s\n", index + 1,
	       status->present_on_off ? "on" : "off");
}

static void level_status(struct bt_mesh_lvl_cli *cli,
			 struct bt_mesh_msg_ctx *ctx,
			 const struct bt_mesh_lvl_status *status)
{
	struct bt_mesh_silvair_enocean_button *button =
		CONTAINER_OF(cli, struct bt_mesh_silvair_enocean_button, lvl);
	int index = button - &silvair_enocean.buttons[0];

	dk_set_led(index, status->current != BT_MESH_LVL_MIN);

	printk("Button %d: Received level response: current: %d\n", index + 1,
		status->current);
}

/* Set up a repeating delayed work to blink the DK's LEDs when attention is
 * requested.
 */
static struct k_work_delayable attention_blink_work;
static bool attention;

static void attention_blink(struct k_work *work)
{
	static int i;

	if (attention) {
		lc_pwm_led_set(i++ % 2 ? 0xFFFF : 0);
		k_work_reschedule(&attention_blink_work, K_MSEC(50));
	} else {
		lc_pwm_led_set(0);
	}
}

static void attention_on(struct bt_mesh_model *mod)
{
	attention = true;
	k_work_reschedule(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	/* Will stop rescheduling blink timer */
	attention = false;
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

#if IS_ENABLED(CONFIG_BT_MESH_NLC_PERF_CONF)
static const uint8_t cmp2_elem_offset1[2] = { 0, 1 };
static const uint8_t cmp2_elem_offset2[2] = { 0, 1 };

static const struct bt_mesh_comp2_record comp_rec[3] = {
	{
	.id = BT_MESH_NLC_PROFILE_ID_DIMMING_CONTROL,
	.version.x = 1,
	.version.y = 0,
	.version.z = 0,
	.elem_offset_cnt = 2,
	.elem_offset = cmp2_elem_offset1,
	.data_len = 0
	},
	{
	.id = BT_MESH_NLC_PROFILE_ID_DIMMING_CONTROL,
	.version.x = 1,
	.version.y = 0,
	.version.z = 0,
	.elem_offset_cnt = 2,
	.elem_offset = cmp2_elem_offset2,
	.data_len = 0
	}
};

static const struct bt_mesh_comp2 comp_p2 = {
	.record_cnt = 3,
	.record = comp_rec
};
#endif


static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV,
				   BT_MESH_MODEL_HEALTH_SRV(&health_srv,
							    &health_pub),
				   BT_MESH_MODEL_DTT_SRV(&dtt_srv),
				   BT_MESH_MODEL_SILVAIR_ENOCEAN_BUTTON(
					&silvair_enocean, 0)),
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_SILVAIR_ENOCEAN_SRV(
					&silvair_enocean))),
	BT_MESH_ELEM(2,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_SILVAIR_ENOCEAN_BUTTON(
					&silvair_enocean, 1)),
		BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

void model_handler_start(void)
{
#if IS_ENABLED(CONFIG_BT_MESH_NLC_PERF_CONF)
	if (bt_mesh_comp2_register(&comp_p2)) {
		printf("Failed to register comp2\n");
	}
#endif
}

const struct bt_mesh_comp *model_handler_init(void)
{
	k_work_init_delayable(&attention_blink_work, attention_blink);

	return &comp;
}
