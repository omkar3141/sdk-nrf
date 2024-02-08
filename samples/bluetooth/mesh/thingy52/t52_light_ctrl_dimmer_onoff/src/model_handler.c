/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include "lc_pwm_led.h"
#include <time.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(model_handler, LOG_LEVEL_INF);

#define PWM_SIZE_STEP 255

#define SHORT_PRESS_THRESHOLD_MS 300
#define LONG_PRESS_TIMEOUT_SEC K_SECONDS(20)
#define DELTA_MOVE_STEP 10000

enum dim_direction { DIM_DOWN, DIM_UP };

/** Context for a single Light Dimmer instance. */
struct dimmer_ctx {
	/** Generic OnOff client instance for this switch */
	struct bt_mesh_onoff_cli onoff_client;
	/** Generic Level client instance for this switch */
	struct bt_mesh_lvl_cli lvl_client;
	/* Relative start time of the latest press of the button */
	int64_t press_start_time;
	/* Dimmer direction */
	enum dim_direction direction;
	/* Long press work (Dimmer mode) */
	struct k_work_delayable long_press_start_work;
	struct k_work_delayable long_press_stop_work;
};

struct lightness_ctx {
	struct bt_mesh_lightness_srv lightness_srv;
	struct k_work_delayable per_work;
	uint16_t target_lvl;
	uint16_t current_lvl;
	uint32_t time_per;
	uint32_t rem_time;
};

#if IS_ENABLED(CONFIG_BT_MESH_NLC_PERF_CONF)
static const uint8_t cmp2_elem_offset1[2] = { 0, 1 };
static const uint8_t cmp2_elem_offset2[1] = { 0 };
static const uint8_t cmp2_elem_offset3[1] = { 0 };

static const struct bt_mesh_comp2_record comp_rec[3] = {
	{
	.id = BT_MESH_NLC_PROFILE_ID_BASIC_LIGHTNESS_CONTROLLER,
	.version.x = 1,
	.version.y = 0,
	.version.z = 0,
	.elem_offset_cnt = 2,
	.elem_offset = cmp2_elem_offset1,
	.data_len = 0
	},
	{
	.id = BT_MESH_NLC_PROFILE_ID_ENERGY_MONITOR, /* Energy Monitor NLC Profile 1.0 */
	.version.x = 1,
	.version.y = 0,
	.version.z = 0,
	.elem_offset_cnt = 1,
	.elem_offset = cmp2_elem_offset2,
	.data_len = 0
	},
	{
	.id = BT_MESH_NLC_PROFILE_ID_DIMMING_CONTROL,
	.version.x = 1,
	.version.y = 0,
	.version.z = 0,
	.elem_offset_cnt = 1,
	.elem_offset = cmp2_elem_offset3,
	.data_len = 0
	}
};

static const struct bt_mesh_comp2 comp_p2 = {
	.record_cnt = 3,
	.record = comp_rec
};
#endif

static void dimmer_start(struct k_work *work)
{
	struct dimmer_ctx *ctx = CONTAINER_OF(k_work_delayable_from_work(work), struct dimmer_ctx,
					      long_press_start_work);

	struct bt_mesh_lvl_move_set move_set = {
		.delta = (ctx->direction == DIM_UP) ? DELTA_MOVE_STEP :
						      (int16_t)(-1 * DELTA_MOVE_STEP),
		.transition = &(struct bt_mesh_model_transition){ .time = 500 },
	};

	LOG_INF("Start dimming %s...", (ctx->direction == DIM_UP ? "UP" : "DOWN"));

	/* As we can't know how many nodes are in a group, it doesn't
	 * make sense to send acknowledged messages to group addresses.
	 */
	if (bt_mesh_model_pub_is_unicast(ctx->lvl_client.model)) {
		bt_mesh_lvl_cli_move_set(&ctx->lvl_client, NULL, &move_set, NULL);
	} else {
		bt_mesh_lvl_cli_move_set_unack(&ctx->lvl_client, NULL, &move_set);
	}

	k_work_reschedule(&ctx->long_press_stop_work, LONG_PRESS_TIMEOUT_SEC);
}

static void dimmer_stop(struct k_work *work)
{
	struct dimmer_ctx *ctx = CONTAINER_OF(k_work_delayable_from_work(work), struct dimmer_ctx,
					      long_press_stop_work);

	struct bt_mesh_lvl_move_set move_set = {
		.delta = 0,
		.transition = NULL,
	};

	if (bt_mesh_model_pub_is_unicast(ctx->lvl_client.model)) {
		bt_mesh_lvl_cli_move_set(&ctx->lvl_client, NULL, &move_set, NULL);
	} else {
		bt_mesh_lvl_cli_move_set_unack(&ctx->lvl_client, NULL, &move_set);
	}
}

static struct dimmer_ctx dimmer = {
	.lvl_client = BT_MESH_LVL_CLI_INIT(NULL),
	.onoff_client = BT_MESH_ONOFF_CLI_INIT(NULL),
	.press_start_time = 0,
	.direction = DIM_DOWN
};

static void dimmer_button_handler(struct dimmer_ctx *ctx, bool pressed)
{
	int err;

	if (pressed) {
		ctx->direction = (ctx->direction == DIM_UP) ? DIM_DOWN : DIM_UP;

		ctx->press_start_time = k_uptime_get();
		err = k_work_reschedule(&ctx->long_press_start_work,
					K_MSEC(SHORT_PRESS_THRESHOLD_MS));

		if (err < 0) {
			LOG_ERR("Scheduling dimming to start failed: %d", err);
		}
	} else {
		if ((k_uptime_get() - ctx->press_start_time) >= SHORT_PRESS_THRESHOLD_MS) {
			/** Button was released after a long time.
			 * Stop dimming immediately.
			 */
			err = k_work_reschedule(&ctx->long_press_stop_work, K_NO_WAIT);

			if (err < 0) {
				LOG_INF("Scheduling dimming to stop failed: %d", err);
			}

		} else {
			/** Button was released after a short time.
			 * Cancel dimming operation and toggle light on/off.
			 */
			LOG_INF("Toggle light %s", (ctx->direction == DIM_UP ? "ON" : "OFF"));
			k_work_cancel_delayable(&ctx->long_press_start_work);

			struct bt_mesh_onoff_set set = {
				.on_off = ctx->direction,
			};

			/* As we can't know how many nodes are in a group, it doesn't
			 * make sense to send acknowledged messages to group addresses.
			 */
			if (bt_mesh_model_pub_is_unicast(ctx->onoff_client.model)) {
				err = bt_mesh_onoff_cli_set(&ctx->onoff_client, NULL, &set, NULL);
			} else {
				err = bt_mesh_onoff_cli_set_unack(&ctx->onoff_client, NULL, &set);
			}

			if (err) {
				LOG_ERR("Failed to send OnOff set message: %d", err);
			}
		}
	}
}

static void button_handler_cb(uint32_t pressed, uint32_t changed)
{
	if (!bt_mesh_is_provisioned()) {
		LOG_ERR("Node is not provisioned");
		return;
	}

	if (changed & BIT(0)) {
		dimmer_button_handler(&dimmer, pressed & BIT(0));
	}
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
	printk("Attention blinking\n");
	k_work_reschedule(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	/* Will stop rescheduling blink timer */
	attention = false;
	k_work_reschedule(&attention_blink_work, K_NO_WAIT);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static void start_new_light_trans(const struct bt_mesh_lightness_set *set,
				  struct lightness_ctx *ctx)
{
	uint32_t step_cnt = abs(set->lvl - ctx->current_lvl) / PWM_SIZE_STEP;
	uint32_t time = set->transition ? set->transition->time : 0;
	uint32_t delay = set->transition ? set->transition->delay : 0;

	ctx->target_lvl = set->lvl;
	ctx->time_per = (step_cnt ? time / step_cnt : 0);
	ctx->rem_time = time;
	k_work_reschedule(&ctx->per_work, K_MSEC(delay));

	printk("New light transition-> Lvl: %d, Time: %d, Delay: %d\n",
	       set->lvl, time, delay);
}

static void periodic_led_work(struct k_work *work)
{
	struct lightness_ctx *l_ctx =
		CONTAINER_OF(work, struct lightness_ctx, per_work);
	l_ctx->rem_time -= l_ctx->time_per;

	if ((l_ctx->rem_time <= l_ctx->time_per) ||
	    (abs(l_ctx->target_lvl - l_ctx->current_lvl) <= PWM_SIZE_STEP)) {
		struct bt_mesh_lightness_status status = {
			.current = l_ctx->target_lvl,
			.target = l_ctx->target_lvl,
		};

		l_ctx->current_lvl = l_ctx->target_lvl;
		l_ctx->rem_time = 0;

		bt_mesh_lightness_srv_pub(&l_ctx->lightness_srv, NULL, &status);

		goto apply_and_print;
	} else if (l_ctx->target_lvl > l_ctx->current_lvl) {
		l_ctx->current_lvl += PWM_SIZE_STEP;
	} else {
		l_ctx->current_lvl -= PWM_SIZE_STEP;
	}

	k_work_reschedule(&l_ctx->per_work, K_MSEC(l_ctx->time_per));
apply_and_print:
	uint16_t clamped_lvl = bt_mesh_lightness_clamp(&l_ctx->lightness_srv,
						       l_ctx->current_lvl);
	lc_pwm_led_set(clamped_lvl);
	printk("Current light lvl: %u/65535\n", clamped_lvl);
}

static void light_set(struct bt_mesh_lightness_srv *srv,
		      struct bt_mesh_msg_ctx *ctx,
		      const struct bt_mesh_lightness_set *set,
		      struct bt_mesh_lightness_status *rsp)
{
	struct lightness_ctx *l_ctx =
		CONTAINER_OF(srv, struct lightness_ctx, lightness_srv);

	start_new_light_trans(set, l_ctx);
	rsp->current = l_ctx->rem_time ? l_ctx->current_lvl : l_ctx->target_lvl;
	rsp->target = l_ctx->target_lvl;
	rsp->remaining_time = set->transition ? set->transition->time : 0;
}

static void light_get(struct bt_mesh_lightness_srv *srv,
		      struct bt_mesh_msg_ctx *ctx,
		      struct bt_mesh_lightness_status *rsp)
{
	struct lightness_ctx *l_ctx =
		CONTAINER_OF(srv, struct lightness_ctx, lightness_srv);

	rsp->current = bt_mesh_lightness_clamp(&l_ctx->lightness_srv, l_ctx->current_lvl);
	rsp->target = l_ctx->target_lvl;
	rsp->remaining_time = l_ctx->rem_time;
}

static const struct bt_mesh_lightness_srv_handlers lightness_srv_handlers = {
	.light_set = light_set,
	.light_get = light_get,
};

static struct lightness_ctx my_ctx = {
	.lightness_srv = BT_MESH_LIGHTNESS_SRV_INIT(&lightness_srv_handlers),

};

static int dummy_energy_use;

static int energy_use_get(struct bt_mesh_sensor_srv *srv,
			 struct bt_mesh_sensor *sensor,
			 struct bt_mesh_msg_ctx *ctx,
			 struct sensor_value *rsp)
{
	/* Report energy usage as dummy value, and increase it by one every time
	 * a get callback is triggered. The logic and hardware for mesuring
	 * the actual energy usage of the device should be implemented here.
	 */
	rsp[0].val1 = dummy_energy_use;
	rsp[0].val2 = 0;

	dummy_energy_use++;

	return 0;
}

static const struct bt_mesh_sensor_descriptor energy_use_desc = {
	.tolerance = {
		.negative = {
			.val1 = 0,
		},
		.positive = {
			.val1 = 0,
		}
	},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_UNSPECIFIED,
	.period = 0,
	.update_interval = 0,
};

static struct bt_mesh_sensor energy_use = {
	.type = &bt_mesh_sensor_precise_tot_dev_energy_use,
	.get = energy_use_get,
	.descriptor = &energy_use_desc,
};

static struct bt_mesh_sensor *const sensors[] = {
	&energy_use,
};

static struct bt_mesh_sensor_srv sensor_srv =
	BT_MESH_SENSOR_SRV_INIT(sensors, ARRAY_SIZE(sensors));

static struct bt_mesh_scene_srv scene_srv;

static struct bt_mesh_light_ctrl_srv light_ctrl_srv =
	BT_MESH_LIGHT_CTRL_SRV_INIT(&my_ctx.lightness_srv);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_CFG_SRV,
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			     BT_MESH_MODEL_LIGHTNESS_SRV(
					 &my_ctx.lightness_srv),
			     BT_MESH_MODEL_SCENE_SRV(&scene_srv),
			     BT_MESH_MODEL_SENSOR_SRV(&sensor_srv),
			     BT_MESH_MODEL_ONOFF_CLI(&dimmer.onoff_client),
			     BT_MESH_MODEL_LVL_CLI(&dimmer.lvl_client)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(2,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_LIGHT_CTRL_SRV(&light_ctrl_srv)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	k_work_init_delayable(&attention_blink_work, attention_blink);
	k_work_init_delayable(&my_ctx.per_work, periodic_led_work);

	static struct button_handler button_handler = {
		.cb = button_handler_cb,
	};

	dk_button_handler_add(&button_handler);
	k_work_init_delayable(&dimmer.long_press_start_work, dimmer_start);
	k_work_init_delayable(&dimmer.long_press_stop_work, dimmer_stop);

	return &comp;
}

void model_handler_start(void)
{
	// int err;

#if IS_ENABLED(CONFIG_BT_MESH_NLC_PERF_CONF)
	if (bt_mesh_comp2_register(&comp_p2)) {
		printf("Failed to register comp2\n");
	}
#endif

	if (bt_mesh_is_provisioned()) {
		return;
	}

	bt_mesh_ponoff_srv_set(&light_ctrl_srv.lightness->ponoff,
			       BT_MESH_ON_POWER_UP_RESTORE);

	/* Do not enable LC Server for demo purposes */
	// err = bt_mesh_light_ctrl_srv_enable(&light_ctrl_srv);
	// if (!err) {
	// 	printk("Successfully enabled LC server\n");
	// }
}
