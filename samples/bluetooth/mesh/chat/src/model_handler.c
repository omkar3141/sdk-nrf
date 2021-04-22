/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>

#include <shell/shell.h>
#include <shell/shell_uart.h>

#include "model_handler.h"

#include <logging/log.h>
LOG_MODULE_DECLARE(scheduler_model);

static const struct shell *scheduler_shell;
static uint16_t net_idx = 0;
static uint16_t app_idx = 0;
static uint16_t remote_addr = 0x100;

static uint8_t dev_key[16] = {1};
static uint8_t net_key[16] = {2};
static uint8_t app_key[16] = {3};
static uint16_t addr = 0x0100;

static uint8_t sch_year = 0;
static uint8_t sch_month = 1;
static uint8_t sch_day = 1;
static uint8_t sch_hour = 0;
static uint8_t sch_minute = 5;
static uint8_t sch_idx = 15;

/******************************************************************************/
/*************************** Health server setup ******************************/
/******************************************************************************/
/* Set up a repeating delayed work to blink the DK's LEDs when attention is
 * requested.
 */
static struct k_delayed_work attention_blink_work;

static void attention_blink(struct k_work *work)
{
	static int idx;
	const uint8_t pattern[] = {
		BIT(0) | BIT(1),
		BIT(1) | BIT(2),
		BIT(2) | BIT(3),
		BIT(3) | BIT(0),
	};
	dk_set_leds(pattern[idx++ % ARRAY_SIZE(pattern)]);
	k_delayed_work_submit(&attention_blink_work, K_MSEC(30));
}

static void attention_on(struct bt_mesh_model *mod)
{
	k_delayed_work_submit(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&attention_blink_work);
	dk_set_leds(DK_NO_LEDS_MSK);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

static void action_set_cb(struct bt_mesh_scheduler_srv *srv,
			struct bt_mesh_msg_ctx *ctx,
			uint8_t idx,
			struct bt_mesh_schedule_entry *entry);

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_ponoff_srv ponoff_srv =
	BT_MESH_PONOFF_SRV_INIT(NULL, NULL, NULL);
static struct bt_mesh_time_srv time_srv = BT_MESH_TIME_SRV_INIT(NULL);
static struct bt_mesh_scene_srv scene_srv;
static struct bt_mesh_scheduler_cli scheduler_cli;
static struct bt_mesh_scheduler_srv scheduler_srv = BT_MESH_SCHEDULER_SRV_INIT(action_set_cb, &time_srv);
static struct bt_mesh_cfg_cli cfg_cli;

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV,
			BT_MESH_MODEL_CFG_CLI(&cfg_cli),
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			BT_MESH_MODEL_SCHEDULER_CLI(&scheduler_cli),
			BT_MESH_MODEL_SCHEDULER_SRV(&scheduler_srv),
			BT_MESH_MODEL_TIME_SRV(&time_srv),
			BT_MESH_MODEL_SCENE_SRV(&scene_srv),
			BT_MESH_MODEL_PONOFF_SRV(&ponoff_srv),
			), BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/******************************************************************************/
/******************************** Scheduler shell **********************************/
/******************************************************************************/
static int cmd_pts_param_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 4) {
		LOG_ERR("Wrong number of command arguments: %d", argc);
		return -EINVAL;
	}

	net_idx = strtol(argv[1], NULL, 0);
	app_idx = strtol(argv[2], NULL, 0);
	remote_addr = strtol(argv[3], NULL, 0);


	shell_print(shell, "Network key idx: %d", net_idx);
	shell_print(shell, "Application key idx: %d", app_idx);
	shell_print(shell, "PTS Rx address: %#2x", remote_addr);
	return 0;
}

static int cmd_get_sched_reg_status(const struct shell *shell, size_t argc, char *argv[])
{
	uint16_t rsp;

	struct bt_mesh_msg_ctx ctx = {
		.net_idx = net_idx,
		.app_idx = app_idx,
		.addr = remote_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT
	};

	int err = bt_mesh_scheduler_cli_get(&scheduler_cli,
					    &ctx,
					    &rsp);

	if (err) {
		LOG_ERR("Failed to send message: %d", err);
	}
	else {
		/* Print received response. */
		shell_print(shell, "Responded schedules: %#2x", rsp);
	}

	return err;
}

static int cmd_get_sched_action_status(const struct shell *shell, size_t argc, char *argv[])
{
	struct bt_mesh_schedule_entry rsp;

	if (argc < 2) {
		return -EINVAL;
	}

	struct bt_mesh_msg_ctx ctx = {
		.net_idx = net_idx,
		.app_idx = app_idx,
		.addr = remote_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT
	};
	uint8_t index = strtol(argv[1], NULL, 0);

	int err = bt_mesh_scheduler_cli_action_get(&scheduler_cli,
					    	   &ctx,
						   index,
						   &rsp);

	if (err) {
		LOG_ERR("Failed to send message: %d", err);
	}
	else {
		/* Print received response. */
		shell_print(shell, "response:");
		shell_print(shell, "action index: %d", index);
		shell_print(shell, "year: %d", rsp.year);
		shell_print(shell, "month: %#2x", rsp.month);
		shell_print(shell, "day: %d", rsp.day);
		shell_print(shell, "hour: %d", rsp.hour);
		shell_print(shell, "minute: %d", rsp.minute);
		shell_print(shell, "second: %d", rsp.second);
		shell_print(shell, "day of week: %#2x", rsp.day_of_week);
		shell_print(shell, "action: %d", rsp.action);
		shell_print(shell, "transition time: %d", rsp.transition_time);
		shell_print(shell, "scene number: %d", rsp.scene_number);
	}

	return err;
}

static int cmd_set_sched_action_status(const struct shell *shell, size_t argc, char *argv[])
{
	struct bt_mesh_schedule_entry rsp;
	struct bt_mesh_schedule_entry param = {
		.year = sch_year,
		.month = sch_month,
		.day = sch_day,
		.hour = sch_hour,
		.minute = sch_minute,
		.second = 60,
		.day_of_week = 0x7F,
		.action = BT_MESH_SCHEDULER_SCENE_RECALL,
		.transition_time = 0,
		.scene_number = 0x0001
	};
	uint8_t index = sch_idx;

	struct bt_mesh_msg_ctx ctx = {
		.net_idx = net_idx,
		.app_idx = app_idx,
		.addr = remote_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT
	};

	int err = bt_mesh_scheduler_cli_action_set(&scheduler_cli,
					    	   &ctx,
						   index,
						   &param,
						   &rsp);

	if (err) {
		LOG_ERR("Failed to send message: %d", err);
	}
	else {
		/* Print received response. */
		shell_print(shell, "response:");
		shell_print(shell, "action index: %d", index);
		shell_print(shell, "year: %d", rsp.year);
		shell_print(shell, "month: %#2x", rsp.month);
		shell_print(shell, "day: %d", rsp.day);
		shell_print(shell, "hour: %d", rsp.hour);
		shell_print(shell, "minute: %d", rsp.minute);
		shell_print(shell, "second: %d", rsp.second);
		shell_print(shell, "day of week: %#2x", rsp.day_of_week);
		shell_print(shell, "action: %d", rsp.action);
		shell_print(shell, "transition time: %d", rsp.transition_time);
		shell_print(shell, "scene number: %d", rsp.scene_number);
	}

	return err;
}

static int cmd_uset_sched_action_status(const struct shell *shell, size_t argc, char *argv[])
{
	struct bt_mesh_schedule_entry param = {
		.year = 17,
		.month = 4,
		.day = 24,
		.hour = 24,
		.minute = 61,
		.second = 60,
		.day_of_week = 16,
		.action = 1,
		.transition_time = 62,
		.scene_number = 4660
	};
	uint8_t index = 0x0f;

	struct bt_mesh_msg_ctx ctx = {
		.net_idx = net_idx,
		.app_idx = app_idx,
		.addr = remote_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT
	};

	int err = bt_mesh_scheduler_cli_action_set_unack(&scheduler_cli,
					    	   	 &ctx,
							 index,
							 &param);

	if (err) {
		LOG_ERR("Failed to send message: %d", err);
	}

	return err;
}

static int cmd_run_srv_scheduler(const struct shell *shell, size_t argc, char *argv[])
{
	static struct bt_mesh_time_status status = { 0 };

	int64_t current_uptime = k_uptime_get();
	status.tai.sec = current_uptime;
	bt_mesh_time_srv_time_set(&time_srv, current_uptime, &status);

	return bt_mesh_scheduler_srv_backdoor(&scheduler_srv);
}

static int cmd_time_set(const struct shell *shell, size_t argc, char *argv[])
{
	static struct bt_mesh_time_status status = { 0 };

	int64_t current_uptime = k_uptime_get();

	status.tai.sec = current_uptime/1000;
	bt_mesh_time_srv_time_set(&time_srv, current_uptime, &status);
	bt_mesh_scheduler_srv_time_update(&scheduler_srv);

	return 0;
}

static int cmd_sh_year_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 2) {
		return -EINVAL;
	}

	sch_year = strtol(argv[1], NULL, 0);
	return 0;
}

static int cmd_sh_month_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 2) {
		return -EINVAL;
	}

	sch_month = strtol(argv[1], NULL, 0);
	return 0;
}

static int cmd_sh_day_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 2) {
		return -EINVAL;
	}

	sch_day = strtol(argv[1], NULL, 0);
	return 0;
}

static int cmd_sh_hour_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 2) {
		return -EINVAL;
	}

	sch_hour = strtol(argv[1], NULL, 0);
	return 0;
}

static int cmd_sh_minute_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 2) {
		return -EINVAL;
	}

	sch_minute = strtol(argv[1], NULL, 0);
	return 0;
}

static int cmd_sh_idx_set(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc < 2) {
		return -EINVAL;
	}

	sch_idx = strtol(argv[1], NULL, 0);
	return 0;
}

static int cmd_vars_print(const struct shell *shell, size_t argc, char *argv[])
{
	shell_print(shell, "remote_addr: 0x%04x", remote_addr);
	shell_print(shell, "sch_year: %d", sch_year);
	shell_print(shell, "sch_month: %d", sch_month);
	shell_print(shell, "sch_day: %d", sch_day);
	shell_print(shell, "sch_hour: %d", sch_hour);
	shell_print(shell, "sch_minute: %d", sch_minute);
	return 0;
}

static int cmd_prov(const struct shell *shell, size_t argc, char *argv[])
{
	uint8_t status;
	int err = bt_mesh_provision(net_key, 0, 0, 0, addr, dev_key);

	if (err) {
		LOG_ERR("Failed to self provision: %d", err);
		return err;
	}

	remote_addr = addr;
	net_idx = 0;
	app_idx = 0;

	/* Add Application Key */
	err = bt_mesh_cfg_app_key_add(net_idx, addr, net_idx, app_idx, app_key, &status);

	if (err) {
		LOG_ERR("Failed to add app key: %d", err);
		return err;
	}

	/* Bind to Health model */
	err = bt_mesh_cfg_mod_app_bind(net_idx, addr, addr, app_idx,
				       BT_MESH_MODEL_ID_SCHEDULER_CLI, &status);
	if (err) {
		LOG_ERR("Failed to bind app key to SCHEDULER_CLI: %d", err);
		return err;
	}

	err = bt_mesh_cfg_mod_app_bind(net_idx, addr, addr, app_idx,
				 BT_MESH_MODEL_ID_SCHEDULER_SRV, &status);
	if (err) {
		LOG_ERR("Failed to bind app key to SCHEDULER_CLI: %d", err);
		return err;
	}

	err = bt_mesh_cfg_mod_app_bind(net_idx, addr, addr, app_idx,
				 BT_MESH_MODEL_ID_SCHEDULER_SETUP_SRV, &status);
	if (err) {
		LOG_ERR("Failed to bind app key to SCHEDULER_CLI: %d", err);
		return err;
	}

	struct bt_mesh_cfg_mod_pub pub = {
		.addr = addr,
		.app_idx = app_idx,
		.ttl = 5,
		.period = 0,
	};

	err = bt_mesh_cfg_mod_pub_set(net_idx, addr, addr, BT_MESH_MODEL_ID_SCHEDULER_CLI,
				    &pub, &status);

	if (err) {
		LOG_ERR("Failed to set pub settings for SCHEDULER_CLI: %d", err);
	}

	return err;
}

SHELL_STATIC_SUBCMD_SET_CREATE(scheduler_cmds,
	SHELL_CMD(prov, NULL, "Provision the device with some settings", cmd_prov),
	SHELL_CMD_ARG(pts_param, NULL, "Provide PTS toolchain parameters <NetIdx> <AppIdx> <PTS Rx addr>", cmd_pts_param_set, 4, 0),
	SHELL_CMD(status_get, NULL, "Scheduler register status get", cmd_get_sched_reg_status),
	SHELL_CMD_ARG(action_get, NULL, "Scheduler action status get <action index>", cmd_get_sched_action_status, 2, 0),
	SHELL_CMD(action_set, NULL, "Scheduler action status set (settings hardcoded)", cmd_set_sched_action_status),
	SHELL_CMD(action_uset, NULL, "Scheduler action status set unack (settings hardcoded)", cmd_uset_sched_action_status),
	SHELL_CMD(scheduler_run, NULL, "Run server scheduler with hardcoded entry", cmd_run_srv_scheduler),
	SHELL_CMD(time_set, NULL, "Set time server to current uptime", cmd_time_set),
	SHELL_CMD_ARG(year_set, NULL, "Set year", cmd_sh_year_set, 2, 0),
	SHELL_CMD_ARG(month_set, NULL, "Set month", cmd_sh_month_set, 2, 0),
	SHELL_CMD_ARG(day_set, NULL, "Set day", cmd_sh_day_set, 2, 0),
	SHELL_CMD_ARG(hour_set, NULL, "Set hour", cmd_sh_hour_set, 2, 0),
	SHELL_CMD_ARG(minute_set, NULL, "Set minute", cmd_sh_minute_set, 2, 0),
	SHELL_CMD_ARG(idx_set, NULL, "Set minute", cmd_sh_idx_set, 2, 0),
	SHELL_CMD(vars, NULL, "Print vars", cmd_vars_print),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(shm, &scheduler_cmds, "Bluetooth Mesh Scheduler Client commands", NULL);


static void action_set_cb(struct bt_mesh_scheduler_srv *srv,
			struct bt_mesh_msg_ctx *ctx,
			uint8_t idx,
			struct bt_mesh_schedule_entry *entry)
{
	// todo logging
}

/******************************************************************************/
/******************************** Public API **********************************/
/******************************************************************************/
const struct bt_mesh_comp *model_handler_init(void)
{
	k_delayed_work_init(&attention_blink_work, attention_blink);

	scheduler_shell = shell_backend_uart_get_ptr();
	shell_print(scheduler_shell, ">>> Bluetooth Mesh Scheduler PTS test suite <<<");

	return &comp;
}
