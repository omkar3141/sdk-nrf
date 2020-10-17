/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
/**
 * @file
 * @brief Internal Light HSL API
 */

#ifndef BT_MESH_INTERNAL_LIGHT_HSL_H__
#define BT_MESH_INTERNAL_LIGHT_HSL_H__

#include <bluetooth/mesh/light_hsl_sat_srv.h>
#include "model_utils.h"

#define LVL_TO_SATUR(_lvl) ((_lvl) + 32768)
#define SATUR_TO_LVL(_satur) ((_satur) - 32768)

static inline uint16_t set_saturation(struct bt_mesh_hsl_sat_srv *srv,
				uint16_t satur_val)
{
	if (satur_val < srv->range.min) {
		return srv->range.min;
	} else if (satur_val > srv->range.max) {
		return srv->range.max;
	} else {
		return satur_val;
	}
}

#endif /* BT_MESH_INTERNAL_LIGHT_HSL_H__ */
