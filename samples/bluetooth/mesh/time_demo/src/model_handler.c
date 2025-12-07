/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/time_srv.h>
#include <dk_buttons_and_leds.h>
#include <time.h>
#include "../../../../subsys/bluetooth/mesh/time_util.h"
#include "model_handler.h"

LOG_MODULE_REGISTER(model_handler, LOG_LEVEL_INF);

/* Time Server and Client instances */
static void time_srv_update_cb(struct bt_mesh_time_srv *srv,
			       struct bt_mesh_msg_ctx *ctx,
			       enum bt_mesh_time_update_types type);

static void time_cli_status_cb(struct bt_mesh_time_cli *cli,
			       struct bt_mesh_msg_ctx *ctx,
			       const struct bt_mesh_time_status *status);

static void time_cli_zone_status_cb(struct bt_mesh_time_cli *cli,
				    struct bt_mesh_msg_ctx *ctx,
				    const struct bt_mesh_time_zone_status *status);

static void time_cli_tai_utc_delta_status_cb(struct bt_mesh_time_cli *cli,
					     struct bt_mesh_msg_ctx *ctx,
					     const struct bt_mesh_time_tai_utc_delta_status *status);

static void time_cli_role_status_cb(struct bt_mesh_time_cli *cli,
				    struct bt_mesh_msg_ctx *ctx,
				    enum bt_mesh_time_role time_role);

static struct bt_mesh_time_srv time_srv = BT_MESH_TIME_SRV_INIT(time_srv_update_cb);

static const struct bt_mesh_time_cli_handlers time_cli_handlers = {
	.time_status = time_cli_status_cb,
	.time_zone_status = time_cli_zone_status_cb,
	.tai_utc_delta_status = time_cli_tai_utc_delta_status_cb,
	.time_role_status = time_cli_role_status_cb,
};

static struct bt_mesh_time_cli time_cli = BT_MESH_TIME_CLI_INIT(&time_cli_handlers);

/* Health Server - Attention callbacks with LED blinking */
static struct k_work_delayable attention_blink_work;
static bool attention_active;

static void attention_blink(struct k_work *work)
{
	static int idx;
	static const uint8_t pattern[] = {
		BIT(DK_LED1),
		BIT(DK_LED2),
		BIT(DK_LED3),
		BIT(DK_LED4),
	};

	if (attention_active) {
		dk_set_leds(pattern[idx++ % ARRAY_SIZE(pattern)]);
		k_work_reschedule(&attention_blink_work, K_MSEC(30));
	} else {
		/* Turn off all LEDs when attention ends */
		dk_set_leds(DK_NO_LEDS_MSK);
	}
}

static void health_srv_attention_on(const struct bt_mesh_model *model)
{
	LOG_INF("Health Server attention ON");
	attention_active = true;
	k_work_reschedule(&attention_blink_work, K_NO_WAIT);
}

static void health_srv_attention_off(const struct bt_mesh_model *model)
{
	LOG_INF("Health Server attention OFF");
	/* Will stop rescheduling blink timer */
	attention_active = false;
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = health_srv_attention_on,
	.attn_off = health_srv_attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

/* Composition data */
static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_CFG_SRV,
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			     BT_MESH_MODEL_TIME_SRV(&time_srv)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_TIME_CLI(&time_cli)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/* Time Server update callback */
static void time_srv_update_cb(struct bt_mesh_time_srv *srv,
			       struct bt_mesh_msg_ctx *ctx,
			       enum bt_mesh_time_update_types type)
{
	switch (type) {
	case BT_MESH_TIME_SRV_STATUS_UPDATE:
		LOG_INF("Time Server: Status update received");
		break;
	case BT_MESH_TIME_SRV_SET_UPDATE:
		LOG_INF("Time Server: Set update received");
		break;
	case BT_MESH_TIME_SRV_ZONE_UPDATE:
		LOG_INF("Time Server: Zone update received");
		break;
	case BT_MESH_TIME_SRV_UTC_UPDATE:
		LOG_INF("Time Server: UTC update received");
		break;
	default:
		LOG_WRN("Unknown update type: %d", type);
		break;
	}
}

/* Time Client status callbacks */
static void time_cli_status_cb(struct bt_mesh_time_cli *cli,
			       struct bt_mesh_msg_ctx *ctx,
			       const struct bt_mesh_time_status *status)
{
	LOG_INF("Time Status received from 0x%04x:", ctx->addr);
	LOG_INF("  TAI seconds: %llu", (uint64_t)status->tai.sec);
	LOG_INF("  TAI subseconds: %u/256", status->tai.subsec);
	LOG_INF("  Uncertainty: %llu ms", status->uncertainty);
	LOG_INF("  TAI-UTC Delta: %d s", status->tai_utc_delta);
	LOG_INF("  Time Zone Offset: %d * 15 min", status->time_zone_offset);
	LOG_INF("  Authority: %s", status->is_authority ? "yes" : "no");

	/* Convert TAI to local time and print */
	int64_t uptime = k_uptime_get();
	struct tm timeptr;
	struct tm *local_time = bt_mesh_time_srv_localtime_r(&time_srv, uptime, &timeptr);

	if (local_time) {
		LOG_INF("  Local time: %04d-%02d-%02d %02d:%02d:%02d",
			local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
			local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
	}
}

static void time_cli_zone_status_cb(struct bt_mesh_time_cli *cli,
				    struct bt_mesh_msg_ctx *ctx,
				    const struct bt_mesh_time_zone_status *status)
{
	LOG_INF("Time Zone Status received from 0x%04x:", ctx->addr);
	LOG_INF("  Current offset: %d * 15 min", status->current_offset);
	LOG_INF("  New offset: %d * 15 min", status->time_zone_change.new_offset);
	LOG_INF("  Change timestamp: %llu", status->time_zone_change.timestamp);
}

static void time_cli_tai_utc_delta_status_cb(struct bt_mesh_time_cli *cli,
					     struct bt_mesh_msg_ctx *ctx,
					     const struct bt_mesh_time_tai_utc_delta_status *status)
{
	LOG_INF("TAI-UTC Delta Status received from 0x%04x:", ctx->addr);
	LOG_INF("  Current delta: %d s", status->delta_current);
	LOG_INF("  New delta: %d s", status->tai_utc_change.delta_new);
	LOG_INF("  Change timestamp: %llu", status->tai_utc_change.timestamp);
}

static void time_cli_role_status_cb(struct bt_mesh_time_cli *cli,
				    struct bt_mesh_msg_ctx *ctx,
				    enum bt_mesh_time_role time_role)
{
	const char *role_str;

	switch (time_role) {
	case BT_MESH_TIME_NONE:
		role_str = "None";
		break;
	case BT_MESH_TIME_AUTHORITY:
		role_str = "Authority";
		break;
	case BT_MESH_TIME_RELAY:
		role_str = "Relay";
		break;
	case BT_MESH_TIME_CLIENT:
		role_str = "Client";
		break;
	default:
		role_str = "Unknown";
		break;
	}

	LOG_INF("Time Role Status received from 0x%04x: %s", ctx->addr, role_str);
}

/* Shell command handlers */
static int cmd_role_set(const struct shell *shell, size_t argc, char *argv[])
{
	enum bt_mesh_time_role role;
	int role_val;

	if (argc != 2) {
		shell_error(shell, "Usage: time_role set <0|1|2>");
		shell_print(shell, "  0 = Authority, 1 = Client, 2 = Relay");
		return -EINVAL;
	}

	role_val = strtol(argv[1], NULL, 0);

	switch (role_val) {
	case 0:
		role = BT_MESH_TIME_AUTHORITY;
		break;
	case 1:
		role = BT_MESH_TIME_CLIENT;
		break;
	case 2:
		role = BT_MESH_TIME_RELAY;
		break;
	default:
		shell_error(shell, "Invalid role: %d. Use 0 (Authority), 1 (Client), or 2 (Relay)",
			    role_val);
		return -EINVAL;
	}

	bt_mesh_time_srv_role_set(&time_srv, role);

	/* Update LEDs to reflect the new role */
	update_role_leds();

	shell_print(shell, "Time role set to: %s",
		    role == BT_MESH_TIME_AUTHORITY ? "Authority" :
		    role == BT_MESH_TIME_CLIENT ? "Client" : "Relay");

	return 0;
}

static int cmd_role_get(const struct shell *shell, size_t argc, char *argv[])
{
	enum bt_mesh_time_role role = time_srv.data.role;
	const char *role_str;

	switch (role) {
	case BT_MESH_TIME_NONE:
		role_str = "None";
		break;
	case BT_MESH_TIME_AUTHORITY:
		role_str = "Authority";
		break;
	case BT_MESH_TIME_RELAY:
		role_str = "Relay";
		break;
	case BT_MESH_TIME_CLIENT:
		role_str = "Client";
		break;
	default:
		role_str = "Unknown";
		break;
	}

	shell_print(shell, "Current time role: %s", role_str);
	return 0;
}

static int cmd_time_set(const struct shell *shell, size_t argc, char *argv[])
{
	struct tm timeptr = {0};
	int day, month, year, hour, min, sec;
	int ret;
	enum bt_mesh_time_role role;
	bool is_authority;

	if (argc != 2) {
		shell_error(shell, "Usage: time set <DD-MM-YYYY:HH-MM-SS>");
		shell_print(shell, "Example: time set 06-12-2025:14-30-00");
		return -EINVAL;
	}

	/* Parse the date/time string */
	ret = sscanf(argv[1], "%d-%d-%d:%d-%d-%d", &day, &month, &year, &hour, &min, &sec);
	if (ret != 6) {
		shell_error(shell, "Invalid format. Use DD-MM-YYYY:HH-MM-SS");
		return -EINVAL;
	}

	/* Validate ranges */
	if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900 ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59) {
		shell_error(shell, "Invalid date/time values");
		return -EINVAL;
	}

	/* Fill in struct tm */
	timeptr.tm_mday = day;
	timeptr.tm_mon = month - 1;  /* tm_mon is 0-11 */
	timeptr.tm_year = year - 1900;  /* tm_year is years since 1900 */
	timeptr.tm_hour = hour;
	timeptr.tm_min = min;
	timeptr.tm_sec = sec;

	/* Convert struct tm to TAI seconds using Time Server utility function */
	struct bt_mesh_time_tai tai;
	ret = ts_to_tai(&tai, &timeptr);
	if (ret != 0) {
		shell_error(shell, "Failed to convert time to TAI: %d", ret);
		return -EINVAL;
	}

	role = model_handler_role_get();
	is_authority = (role == BT_MESH_TIME_AUTHORITY);

	/* Set the time on the server */
	struct bt_mesh_time_status status = {
		.tai = tai,
		.uncertainty = 0,
		.tai_utc_delta = 37,  /* Current TAI-UTC delta (as of 2025) */
		.time_zone_offset = 0,  /* UTC */
		.is_authority = is_authority,
	};

	/* ts_to_tai treats input as TAI, but we're providing UTC time.
	 * Need to add TAI-UTC delta to convert from UTC to TAI.
	 */
	status.tai.sec += status.tai_utc_delta;

	bt_mesh_time_srv_time_set(&time_srv, k_uptime_get(), &status);

	/* Publish the new time if provisioned */
	if (bt_mesh_is_provisioned()) {
		ret = bt_mesh_time_srv_time_status_send(&time_srv, NULL);
		if (ret) {
			shell_warn(shell, "Time status publish failed: %d", ret);
		}
	}

	shell_print(shell, "Time set to: %04d-%02d-%02d %02d:%02d:%02d",
		    year, month, day, hour, min, sec);

	return 0;
}

static int cmd_time_get(const struct shell *shell, size_t argc, char *argv[])
{
	int64_t uptime = k_uptime_get();
	struct tm timeptr;
	struct tm *local_time;
	struct bt_mesh_time_status status;
	int ret;

	ret = bt_mesh_time_srv_status(&time_srv, uptime, &status);
	if (ret) {
		shell_error(shell, "Failed to get time status: %d", ret);
		return ret;
	}

	local_time = bt_mesh_time_srv_localtime_r(&time_srv, uptime, &timeptr);
	if (!local_time) {
		shell_error(shell, "Failed to get local time");
		return -EINVAL;
	}

	shell_print(shell, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
		    local_time->tm_year + 1900, local_time->tm_mon + 1,
		    local_time->tm_mday, local_time->tm_hour,
		    local_time->tm_min, local_time->tm_sec);
	shell_print(shell, "TAI seconds: %llu", (uint64_t)status.tai.sec);
	shell_print(shell, "Uncertainty: %llu ms", status.uncertainty);

	return 0;
}

/* Shell subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(time_role_cmds,
	SHELL_CMD_ARG(set, NULL, "Set time role: 0=Authority, 1=Client, 2=Relay", cmd_role_set, 2, 0),
	SHELL_CMD_ARG(get, NULL, "Get current time role", cmd_role_get, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(time_cmds,
	SHELL_CMD_ARG(set, NULL, "Set time <DD-MM-YYYY:HH-MM-SS>", cmd_time_set, 2, 0),
	SHELL_CMD_ARG(get, NULL, "Get current time", cmd_time_get, 1, 0),
	SHELL_SUBCMD_SET_END
);

/* Register root commands */
SHELL_CMD_REGISTER(time_role, &time_role_cmds, "Time role commands (0=Authority, 1=Client, 2=Relay)", NULL);
SHELL_CMD_REGISTER(time, &time_cmds, "Time commands", NULL);

/* Public API functions */
int model_handler_init(void)
{
	k_work_init_delayable(&attention_blink_work, attention_blink);
	LOG_INF("Model handler initialized");
	return 0;
}

void model_handler_set_default_time_if_needed(void)
{
	int64_t uptime = k_uptime_get();
	struct bt_mesh_time_status status;
	int ret;

	/* Check if time is already set */
	ret = bt_mesh_time_srv_status(&time_srv, uptime, &status);
	if (ret == 0) {
		/* Time is already set, don't overwrite */
		return;
	}

	/* Set default time: 01-01-2025:00-00-00 */
	struct tm timeptr = {0};
	timeptr.tm_mday = 1;
	timeptr.tm_mon = 0;  /* January (0-11) */
	timeptr.tm_year = 2025 - 1900;  /* 2025 */
	timeptr.tm_hour = 0;
	timeptr.tm_min = 0;
	timeptr.tm_sec = 0;

	/* Convert struct tm to TAI seconds using Time Server utility function */
	struct bt_mesh_time_tai tai;
	ret = ts_to_tai(&tai, &timeptr);
	if (ret != 0) {
		LOG_WRN("Failed to convert default time to TAI: %d", ret);
		return;
	}

	enum bt_mesh_time_role role = model_handler_role_get();
	bool is_authority = (role == BT_MESH_TIME_AUTHORITY);

	/* Set the default time on the server */
	struct bt_mesh_time_status default_status = {
		.tai = tai,
		.uncertainty = 0,
		.tai_utc_delta = 37,  /* Current TAI-UTC delta (as of 2025) */
		.time_zone_offset = 0,  /* UTC */
		.is_authority = is_authority,
	};

	/* ts_to_tai treats input as TAI, but we're providing UTC time.
	 * Need to add TAI-UTC delta to convert from UTC to TAI.
	 */
	default_status.tai.sec += default_status.tai_utc_delta;

	bt_mesh_time_srv_time_set(&time_srv, uptime, &default_status);
	LOG_INF("Default time set to: 2025-01-01 00:00:00");
}

const struct bt_mesh_comp *model_handler_comp_data_get(void)
{
	return &comp;
}

void model_handler_role_set(enum bt_mesh_time_role role)
{
	bt_mesh_time_srv_role_set(&time_srv, role);

	/* Update LEDs to reflect the new role */
	update_role_leds();
}

enum bt_mesh_time_role model_handler_role_get(void)
{
	return time_srv.data.role;
}

int model_handler_time_set(int day, int month, int year, int hour, int min, int sec)
{
	struct tm timeptr = {0};
	enum bt_mesh_time_role role;
	bool is_authority;
	int ret;

	/* Validate input */
	if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900 ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59) {
		LOG_ERR("Invalid date/time values");
		return -EINVAL;
	}

	/* Fill in struct tm */
	timeptr.tm_mday = day;
	timeptr.tm_mon = month - 1;
	timeptr.tm_year = year - 1900;
	timeptr.tm_hour = hour;
	timeptr.tm_min = min;
	timeptr.tm_sec = sec;

	/* Convert struct tm to TAI seconds using Time Server utility function */
	struct bt_mesh_time_tai tai;
	ret = ts_to_tai(&tai, &timeptr);
	if (ret != 0) {
		LOG_ERR("Failed to convert time to TAI: %d", ret);
		return -EINVAL;
	}

	role = model_handler_role_get();
	is_authority = (role == BT_MESH_TIME_AUTHORITY);

	/* Set the time on the server */
	struct bt_mesh_time_status status = {
		.tai = tai,
		.uncertainty = 0,
		.tai_utc_delta = 37,
		.time_zone_offset = 0,
		.is_authority = is_authority,
	};

	/* ts_to_tai treats input as TAI, but we're providing UTC time.
	 * Need to add TAI-UTC delta to convert from UTC to TAI.
	 */
	status.tai.sec += status.tai_utc_delta;

	bt_mesh_time_srv_time_set(&time_srv, k_uptime_get(), &status);

	LOG_INF("Time set to: %04d-%02d-%02d %02d:%02d:%02d",
		year, month, day, hour, min, sec);

	/* Publish the new time if provisioned */
	if (bt_mesh_is_provisioned()) {
		ret = bt_mesh_time_srv_time_status_send(&time_srv, NULL);
		if (ret) {
			LOG_WRN("Time status publish failed: %d", ret);
		}
	}

	return 0;
}

void model_handler_time_print(void)
{
	int64_t uptime = k_uptime_get();
	struct tm timeptr;
	struct tm *local_time;
	struct bt_mesh_time_status status;
	int ret;

	ret = bt_mesh_time_srv_status(&time_srv, uptime, &status);
	if (ret) {
		LOG_WRN("Time not set or invalid");
		return;
	}

	local_time = bt_mesh_time_srv_localtime_r(&time_srv, uptime, &timeptr);
	if (!local_time) {
		LOG_WRN("Failed to get local time");
		return;
	}

	LOG_INF("Current time: %04d-%02d-%02d %02d:%02d:%02d",
		local_time->tm_year + 1900, local_time->tm_mon + 1,
		local_time->tm_mday, local_time->tm_hour,
		local_time->tm_min, local_time->tm_sec);
}
