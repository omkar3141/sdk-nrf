/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_init, CONFIG_LOG_DEFAULT_LEVEL);

void start_smp_bluetooth_adverts(void);

static void ble_init_task(void)
{
	start_smp_bluetooth_adverts();
}

K_THREAD_DEFINE(ble_init_task_id, 2048, ble_init_task, NULL, NULL, NULL, 5, 0, 0);
