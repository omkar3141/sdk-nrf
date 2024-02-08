/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bluetooth/mesh/models.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_sx1509b.h>

#if DT_NODE_HAS_STATUS(DT_NODELABEL(sx1509b), okay)
	/* Thingy52 detected */
#else
	#error "Unsupported board: sx1509b devicetree alias is not defined"
#endif

#define PWM_PERIOD 255

#define NUMBER_OF_LEDS 3
#define GREEN_LED DT_GPIO_PIN(DT_NODELABEL(led0), gpios)
#define BLUE_LED DT_GPIO_PIN(DT_NODELABEL(led1), gpios)
#define RED_LED DT_GPIO_PIN(DT_NODELABEL(led2), gpios)

const struct device *sx1509b_dev;

static const gpio_pin_t rgb_pins[] = {
	RED_LED,
	GREEN_LED,
	BLUE_LED,
};

void lc_pwm_led_init(void)
{
	sx1509b_dev = DEVICE_DT_GET(DT_NODELABEL(sx1509b));

	if (!device_is_ready(sx1509b_dev)) {
		printk("sx1509b: device not ready.\n");
		return;
	}

	for (int i = 0; i < NUMBER_OF_LEDS; i++) {
		int err = sx1509b_led_intensity_pin_configure(sx1509b_dev,
							  rgb_pins[i]);
		if (err) {
			printk("Error configuring pin for LED intensity\n");
		}
	}
}

void lc_pwm_led_set(uint16_t desired_lvl)
{
	uint32_t scaled_lvl =
		(PWM_PERIOD * desired_lvl) /
		BT_MESH_LIGHTNESS_MAX;

	for (int i = 0; i < NUMBER_OF_LEDS; i++) {
		sx1509b_led_intensity_pin_set(sx1509b_dev, rgb_pins[i], scaled_lvl);
	}
}
