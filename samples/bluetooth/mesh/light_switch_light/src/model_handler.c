/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Model handler: one Generic OnOff Client (button) and one Generic OnOff Server (LED).
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh/proxy.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

#define LED_IDX 0
#define BUTTON_IDX 0

/* OnOff Server (LED) */
static void led_set(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_onoff_set *set,
		    struct bt_mesh_onoff_status *rsp);
static void led_get(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    struct bt_mesh_onoff_status *rsp);

static const struct bt_mesh_onoff_srv_handlers onoff_handlers = {
	.set = led_set,
	.get = led_get,
};

struct led_ctx {
	struct bt_mesh_onoff_srv srv;
	struct k_work_delayable work;
	uint32_t remaining;
	bool value;
};

static struct led_ctx led_ctx = {
	.srv = BT_MESH_ONOFF_SRV_INIT(&onoff_handlers),
};

static void led_transition_start(struct led_ctx *led)
{
	dk_set_led(LED_IDX, true);
	k_work_reschedule(&led->work, K_MSEC(led->remaining));
	led->remaining = 0;
}

static void led_status(struct led_ctx *led, struct bt_mesh_onoff_status *status)
{
	status->remaining_time = led->remaining ? led->remaining :
		k_ticks_to_ms_ceil32(k_work_delayable_remaining_get(&led->work));
	status->target_on_off = led->value;
	status->present_on_off = led->value || status->remaining_time;
}

static void led_set(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_onoff_set *set,
		    struct bt_mesh_onoff_status *rsp)
{
	struct led_ctx *led = CONTAINER_OF(srv, struct led_ctx, srv);

	if (set->on_off == led->value) {
		goto respond;
	}

	led->value = set->on_off;
	if (!bt_mesh_model_transition_time(set->transition)) {
		led->remaining = 0;
		dk_set_led(LED_IDX, set->on_off);
		goto respond;
	}

	led->remaining = set->transition->time;

	if (set->transition->delay) {
		k_work_reschedule(&led->work, K_MSEC(set->transition->delay));
	} else {
		led_transition_start(led);
	}

respond:
	if (rsp) {
		led_status(led, rsp);
	}
}

static void led_get(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    struct bt_mesh_onoff_status *rsp)
{
	struct led_ctx *led = CONTAINER_OF(srv, struct led_ctx, srv);

	led_status(led, rsp);
}

static void led_work(struct k_work *work)
{
	struct led_ctx *led = CONTAINER_OF(work, struct led_ctx, work.work);

	if (led->remaining) {
		led_transition_start(led);
	} else {
		dk_set_led(LED_IDX, led->value);

		struct bt_mesh_onoff_status status;

		led_status(led, &status);
		bt_mesh_onoff_srv_pub(&led->srv, NULL, &status);
	}
}

/* OnOff Client (button) */
static void onoff_status_handler(struct bt_mesh_onoff_cli *cli,
				 struct bt_mesh_msg_ctx *ctx,
				 const struct bt_mesh_onoff_status *status);

static struct bt_mesh_onoff_cli onoff_cli = BT_MESH_ONOFF_CLI_INIT(&onoff_status_handler);

static bool switch_status;

static void onoff_status_handler(struct bt_mesh_onoff_cli *cli,
				 struct bt_mesh_msg_ctx *ctx,
				 const struct bt_mesh_onoff_status *status)
{
	switch_status = status->present_on_off;
	dk_set_led(LED_IDX, status->present_on_off);
	printk("OnOff status: %s\n", status->present_on_off ? "on" : "off");
}

static void button_handler_cb(uint32_t pressed, uint32_t changed)
{
	if (!bt_mesh_is_provisioned()) {
		return;
	}

	if (!(pressed & changed & BIT(BUTTON_IDX))) {
		return;
	}

	struct bt_mesh_onoff_set set = {
		.on_off = !switch_status,
	};
	int err;

	if (bt_mesh_model_pub_is_unicast(onoff_cli.model)) {
		err = bt_mesh_onoff_cli_set(&onoff_cli, NULL, &set, NULL);
	} else {
		err = bt_mesh_onoff_cli_set_unack(&onoff_cli, NULL, &set);
		if (!err) {
			switch_status = set.on_off;
			dk_set_led(LED_IDX, set.on_off);
		}
	}

	if (err) {
		printk("OnOff set failed: %d\n", err);
	}
}

/* Attention (Health Server) */
static struct k_work_delayable attention_blink_work;
static bool attention;

static void attention_blink(struct k_work *work)
{
	static bool blink_on;

	if (attention) {
		dk_set_led(LED_IDX, blink_on);
		blink_on = !blink_on;
		k_work_reschedule(&attention_blink_work, K_MSEC(30));
	} else {
		dk_set_led(LED_IDX, false);
	}
}

static void attention_on(const struct bt_mesh_model *mod)
{
	(void)mod;
	attention = true;
	k_work_reschedule(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(const struct bt_mesh_model *mod)
{
	(void)mod;
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

/* Composition */
static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_CFG_SRV,
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			     BT_MESH_MODEL_ONOFF_CLI(&onoff_cli),
			     BT_MESH_MODEL_ONOFF_SRV(&led_ctx.srv)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	static struct button_handler button_handler = {
		.cb = button_handler_cb,
	};

	dk_button_handler_add(&button_handler);
	k_work_init_delayable(&attention_blink_work, attention_blink);
	k_work_init_delayable(&led_ctx.work, led_work);

	return &comp;
}
