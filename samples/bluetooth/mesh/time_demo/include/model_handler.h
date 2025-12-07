/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Model handler for Time Demo sample.
 */

#ifndef MODEL_HANDLER_H__
#define MODEL_HANDLER_H__

#include <bluetooth/mesh/time_srv.h>
#include <bluetooth/mesh/time_cli.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the model handler.
 *
 * @return 0 on success, negative errno on failure.
 */
int model_handler_init(void);

/**
 * @brief Get the composition data.
 *
 * @return Pointer to composition data structure.
 */
const struct bt_mesh_comp *model_handler_comp_data_get(void);

/**
 * @brief Set the time role.
 *
 * @param role Time role to set.
 */
void model_handler_role_set(enum bt_mesh_time_role role);

/**
 * @brief Get the current time role.
 *
 * @return Current time role.
 */
enum bt_mesh_time_role model_handler_role_get(void);

/**
 * @brief Set the current time on the time server.
 *
 * @param day Day of month (1-31).
 * @param month Month (1-12).
 * @param year Full year (e.g., 2025).
 * @param hour Hour (0-23).
 * @param min Minute (0-59).
 * @param sec Second (0-59).
 *
 * @return 0 on success, negative errno on failure.
 */
int model_handler_time_set(int day, int month, int year, int hour, int min, int sec);

/**
 * @brief Print the current time from the time server.
 */
void model_handler_time_print(void);

/**
 * @brief Set default time if time is not already set.
 * Sets time to 01-01-2025:00-00-00 if the time server has no valid time.
 */
void model_handler_set_default_time_if_needed(void);

/**
 * @brief Update role LEDs based on current role.
 * This is implemented in main.c and should be called whenever the role changes.
 */
void update_role_leds(void);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_HANDLER_H__ */
