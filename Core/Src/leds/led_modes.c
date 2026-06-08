#include "led/led_modes.h"
#include "led/led_interface.h"
#include "modbus/circularBuffer.h"
#include "modbus/modbus_config.h"
#include "modbus/modbus_interface.h"
#include "stm32f0xx_hal.h"
#include <stdint.h>

void rainbow(void);
void fade(void);
void police(void);
void kr_color(void);

ModbusInterface_t *hmb;

led_mode_fn_t vegasShow[4] = {&rainbow, &fade, &police, &kr_color};

typedef enum {
	kr_dir_right,
	kr_dir_left
} kr_dir_t;

static void kr_run(uint8_t lednum, rgb_t color, kr_dir_t dir);
static void bootup_step(uint8_t staticPos, uint8_t dynamicPos);
static void bootup_clear_tail(uint8_t staticPos, uint8_t dynamicPos);

void led_bootup(void) {
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		led_set_colorWithBrightness(i, (argb_t){.color = led_black, .brightness = LED_BRIGHTNESS_LOW});
	}
	led_transmit();
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		bootup_step(i, 0);
		bootup_clear_tail(led_buffer.count - 1 - i, 0);
	}

	hmb->ledMode = LED_MODE_NORMAL;
	led_updateMode();
}

void led_turnOff(void) {
	argb_t led_setting = {.color = led_black, .brightness = LED_BRIGHTNESS_LOW};
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		led_set_colorWithBrightness(i, led_setting);
	}
	led_transmit();
}

void led_turnOn(void) {
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		argb_t led_setting;
		if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_ERROR_FLAG) {
			led_setting.color = led_red;
			led_setting.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
		} else if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_NEWOP_FLAG) {
			led_setting.color = led_blue;
			led_setting.brightness = LED_BRIGHTNESS_MEDIUM;
		} else if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_TAKEN_FLAG) {
			led_setting.color = led_green;
			led_setting.brightness = LED_BRIGHTNESS_MEDIUM;
		} else {
			led_setting.color = led_black;
			led_setting.brightness = LED_BRIGHTNESS_LOW;
		}
		led_set_colorWithBrightness(i, led_setting);
	}
	led_transmit();
}

void led_vegas(void) {
	while (hmb->ledMode == LED_MODE_VEGAS) {
		for (int i = 0; i < led_buffer.count && hmb->ledMode == LED_MODE_VEGAS; i++) {
			rgb_t color_holder;
			if (i % 6 == 0) {
				color_holder.r = 0;
				color_holder.g = 0;
				color_holder.b = 64;
			} else if (i % 6 == 1) {
				color_holder.r = 32;
				color_holder.g = 0;
				color_holder.b = 32;
			} else if (i % 6 == 2) {
				color_holder.r = 64;
				color_holder.g = 0;
				color_holder.b = 0;
			} else if (i % 6 == 3) {
				color_holder.r = 32;
				color_holder.g = 32;
				color_holder.b = 0;
			} else if (i % 6 == 4) {
				color_holder.r = 0;
				color_holder.g = 64;
				color_holder.b = 0;
			} else {
				color_holder.r = 0;
				color_holder.g = 32;
				color_holder.b = 32;
			}
			led_set_color(i, color_holder);
		}
		led_transmit();
		for (int i = 0; i < 4 && hmb->ledMode == LED_MODE_VEGAS; i++) {
			vegasShow[i]();
			Modbus_Update(hmb);
			led_updateMode();
		}
	}
}

void led_knight_rider(void) {
	static kr_dir_t dir = kr_dir_left;
	while (hmb->ledMode == LED_MODE_KNIGHT) {
		for (uint8_t i = 0; i < led_buffer.count + 2 && hmb->ledMode == LED_MODE_KNIGHT; i++) {
			kr_run(i, led_red, dir);
			HAL_Delay(37);
		}
		dir = dir == kr_dir_left ? kr_dir_right : kr_dir_left;

		Modbus_Update(hmb);
		led_updateMode();
	}
}

void rainbow(void) {
	for (uint8_t i = 0; i < 25 && hmb->ledMode == LED_MODE_VEGAS; i++) {
		for (uint8_t j = 0; j < 6 && hmb->ledMode == LED_MODE_VEGAS; j++) {
			for (uint8_t k = 0; k < 32 && hmb->ledMode == LED_MODE_VEGAS; k++) {
				for (uint8_t l = 0; l < led_buffer.count && hmb->ledMode == LED_MODE_VEGAS; l++) {
					argb_t led_setting;
					led_get_color(l, NULL, &led_setting.color.r, &led_setting.color.g, &led_setting.color.b);
					if ((j + l) % 6 == 0) {
						led_setting.color.r++;
						led_setting.color.b--;
					} else if ((j + l) % 6 == 1) {
						led_setting.color.r++;
						led_setting.color.b--;
					} else if ((j + l) % 6 == 2) {
						led_setting.color.g++;
						led_setting.color.r--;
					} else if ((j + l) % 6 == 3) {
						led_setting.color.g++;
						led_setting.color.r--;
					} else if ((j + l) % 6 == 4) {
						led_setting.color.b++;
						led_setting.color.g--;
					} else {
						led_setting.color.b++;
						led_setting.color.g--;
					}
					led_set_color(l, led_setting.color);
				}
				HAL_Delay(5);
				led_transmit();
			}
		}
	}
}

void fade_brighten(uint8_t position, uint8_t statement) {
	rgb_t led_setting;
	led_get_color(position, NULL, &led_setting.r, &led_setting.g, &led_setting.b);
	if (statement == 0) {
		if (led_setting.b <= 253)
			led_setting.b += 2;
		else
			led_setting.b = 255;
	} else if (statement == 1) {
		if (led_setting.b < 255)
			led_setting.b++;
		else
			led_setting.b = 255;
		if (led_setting.r < 255)
			led_setting.r++;
		else
			led_setting.r = 255;
	} else if (statement == 2) {
		if (led_setting.r <= 253)
			led_setting.r += 2;
		else
			led_setting.r = 255;
	} else if (statement == 3) {
		if (led_setting.r < 255)
			led_setting.r++;
		else
			led_setting.r = 255;
		if (led_setting.g < 255)
			led_setting.g++;
		else
			led_setting.g = 255;
	} else if (statement == 4) {
		if (led_setting.g <= 253)
			led_setting.g += 2;
		else
			led_setting.g = 255;
	} else {
		if (led_setting.g < 255)
			led_setting.g++;
		else
			led_setting.g = 255;
		if (led_setting.b < 255)
			led_setting.b++;
		else
			led_setting.b = 255;
	}
	led_set_color(position, led_setting);
	led_transmit();
}

void fade_darken(uint8_t position, uint8_t statement) {
	rgb_t led_setting;
	led_get_color(position, NULL, &led_setting.r, &led_setting.g, &led_setting.b);

	if (statement == 0) {
		if (led_setting.b >= 2)
			led_setting.b -= 2;
		else
			led_setting.b = 0;
	} else if (statement == 1) {
		if (led_setting.b > 0)
			led_setting.b--;
		else
			led_setting.b = 0;
		if (led_setting.r > 0)
			led_setting.r--;
		else
			led_setting.r = 0;
	} else if (statement == 2) {
		if (led_setting.r >= 2)
			led_setting.r -= 2;
		else
			led_setting.r = 0;
	} else if (statement == 3) {
		if (led_setting.r > 0)
			led_setting.r--;
		else
			led_setting.r = 0;
		if (led_setting.g > 0)
			led_setting.g--;
		else
			led_setting.g = 0;
	} else if (statement == 4) {
		if (led_setting.g >= 2)
			led_setting.g -= 2;
		else
			led_setting.g = 0;
	} else {
		if (led_setting.g > 0)
			led_setting.g--;
		else
			led_setting.g = 0;
		if (led_setting.b > 0)
			led_setting.b--;
		else
			led_setting.b = 0;
	}
	led_set_color(position, led_setting);
	led_transmit();
}

void fade(void) {
	for (uint8_t j = 0; j < 32 && hmb->ledMode == LED_MODE_VEGAS; j++) {
		for (uint8_t k = 0; k < led_buffer.count && hmb->ledMode == LED_MODE_VEGAS; k++) {
			fade_darken(k, k % 6);
			HAL_Delay(5);
		}
	}
	for (uint8_t i = 0; i < 6 && hmb->ledMode == LED_MODE_VEGAS; i++) {
		for (uint8_t j = 0; j < 32 && hmb->ledMode == LED_MODE_VEGAS; j++) {
			for (uint8_t k = 0; k < led_buffer.count && hmb->ledMode == LED_MODE_VEGAS; k++) {
				fade_brighten(k, i);
				HAL_Delay(5);
			}
		}
		for (uint8_t j = 0; j < 32 && hmb->ledMode == LED_MODE_VEGAS; j++) {
			for (uint8_t k = 0; k < led_buffer.count && hmb->ledMode == LED_MODE_VEGAS; k++) {
				fade_darken(k, i);
				HAL_Delay(5);
			}
		}
	}
}

void police(void) {
	for (uint8_t i = 0; i < 40 && hmb->ledMode == LED_MODE_VEGAS; i++) {
		for (uint8_t j = 0; j < led_buffer.count; j++) {
			argb_t led_setting = {.brightness = LED_BRIGHTNESS_MEDIUM_HIGH, .color = led_black};
			if (j < led_buffer.count / 2) {
				if (i % 2)
					led_setting.color = led_blue;
				else
					led_setting.color = led_red;
			} else {
				if (i % 2)
					led_setting.color = led_red;
				else
					led_setting.color = led_blue;
			}
			led_set_colorWithBrightness(j, led_setting);
		}
		led_transmit();
		HAL_Delay(200);
	}
}

void kr_color(void) {
	uint8_t incVal = 5;

	rgb_t color;
	color.r = 0;
	color.g = 0;
	color.b = 0;
	kr_dir_t dir = kr_dir_right;
	for (uint8_t repeat = 0; repeat < 6 && hmb->ledMode == LED_MODE_VEGAS; repeat++) {
		for (uint8_t i = 0; i < led_buffer.count + 2 && hmb->ledMode == LED_MODE_VEGAS; i++) {
			color.b += incVal;
			if (color.b >= 255) {
				color.b = 0;
				color.r += incVal;
				if (color.r >= 255) {
					color.r = 0;
					color.g += incVal;
					if (color.g >= 255) {
						color.g = 0;
					}
				}
			}
			kr_run(i, color, dir);
			HAL_Delay(37);
		}
		dir = dir == kr_dir_left ? kr_dir_right : kr_dir_left;
	}
}

static void kr_run(uint8_t lednum, rgb_t color, kr_dir_t dir) {
	argb_t color_holder;
	int16_t head_pos = (dir == kr_dir_left) ? lednum : (int16_t)(led_buffer.count - 1U - lednum);

	for (uint8_t i = 0; i < led_buffer.count && (hmb->ledMode == LED_MODE_KNIGHT || hmb->ledMode == LED_MODE_VEGAS); i++) {
		int16_t distance = head_pos - i;

		if (dir == kr_dir_right) {
			distance = i - head_pos;
		}

		led_get_color(i, NULL, &color_holder.color.r, &color_holder.color.g, &color_holder.color.b);

		if (distance == 0) {
			color_holder.brightness = LED_BRIGHTNESS_HIGH;
			color_holder.color = color;
		} else if (distance == 1) {
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
		} else if (distance == 2) {
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM;
		} else if (distance == 3) {
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM_LOW;
		} else {
			color_holder.brightness = LED_BRIGHTNESS_LOW;
		}

		led_set_colorWithBrightness(i, color_holder);
	}
	led_transmit();
}

static void bootup_step(uint8_t staticPos, uint8_t dynamicPos) {
	if (dynamicPos >= led_buffer.count - staticPos) {
		return;
	}
	for (uint8_t i = 0; i < led_buffer.count - staticPos; i++) {
		argb_t color_holder = {.color = led_orange, .brightness = LED_BRIGHTNESS_LOW};

		if (i == dynamicPos) {
			color_holder.brightness = LED_BRIGHTNESS_HIGH;
		} else if (i == dynamicPos - 1)
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
		else if (i == dynamicPos - 2)
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM;
		else if (i == dynamicPos - 3)
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM_LOW;

		if (i <= dynamicPos)
			led_set_colorWithBrightness(i, color_holder);
	}

	led_transmit();
	HAL_Delay(37);

	bootup_step(staticPos, dynamicPos + 1);
}

static void bootup_clear_tail(uint8_t staticPos, uint8_t dynamicPos) {
	if (dynamicPos == 3) {
		return;
	}

	for (uint8_t i = 0; i < 3 - dynamicPos; i++) {
		argb_t color_holder = {.color = led_orange, .brightness = LED_BRIGHTNESS_LOW};
		color_holder.brightness = LED_BRIGHTNESS_MEDIUM - i - dynamicPos;
		led_set_colorWithBrightness(staticPos - 1 - i, color_holder);
	}

	led_transmit();
	HAL_Delay(37);
	bootup_clear_tail(staticPos, (uint8_t)(dynamicPos + 1));
}