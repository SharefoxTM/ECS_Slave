#include "led/led_modes.h"
#include "led/led_interface.h"
#include "modbus/circularBuffer.h"
#include "modbus/modbus_interface.h"

void rainbow(void);
void fade(void);
void police(void);
void kr_color(void);

ModbusInterface_t *hmb;

led_mode_fn_t vegasShow[4] = {&rainbow, &fade, &police, &kr_color};

typedef enum {
	kr_dir_left,
	kr_dir_right
} kr_dir_t;

void led_turnOff(void) {
	argb_t led_setting = {.color = led_black, .brightness = LED_BRIGHTNESS_LOW};
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		led_set_color(i, led_setting);
	}
}

void led_turnOn(void) {
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		argb_t led_setting = {.color = led_black, .brightness = LED_BRIGHTNESS_LOW};
		if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_ERROR_FLAG)
			led_setting.color = led_red;
		else if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_NEWOP_FLAG)
			led_setting.color = led_blue;
		else if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_TAKEN_FLAG)
			led_setting.color = led_green;

		led_set_color(i, led_setting);
	}
}

void led_vegas(void) {
	while (cbuf_empty(hcbuf_modbus)) {
		for (int i = 0; i < led_buffer.count && cbuf_empty(hcbuf_modbus); i++) {
			argb_t color_holder;
			if (i % 6 == 0) {
				color_holder.color.r = 0;
				color_holder.color.g = 0;
				color_holder.color.b = 64;
			} else if (i % 6 == 1) {
				color_holder.color.r = 32;
				color_holder.color.g = 0;
				color_holder.color.b = 32;
			} else if (i % 6 == 2) {
				color_holder.color.r = 64;
				color_holder.color.g = 0;
				color_holder.color.b = 0;
			} else if (i % 6 == 3) {
				color_holder.color.r = 32;
				color_holder.color.g = 32;
				color_holder.color.b = 0;
			} else if (i % 6 == 4) {
				color_holder.color.r = 0;
				color_holder.color.g = 64;
				color_holder.color.b = 0;
			} else {
				color_holder.color.r = 0;
				color_holder.color.g = 32;
				color_holder.color.b = 32;
			}
			color_holder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
			led_set_color(i, color_holder);
		}
		led_transmit();
		for (int i = 0; i < 4 && cbuf_empty(hcbuf_modbus); i++) {
			vegasShow[i]();
		}
	}
}

void kr_run(uint8_t lednum, kr_dir_t dir) {
	argb_t color_holder;
	color_holder.color = led_red;
	color_holder.brightness = LED_BRIGHTNESS_LOW;
	if (dir == kr_dir_right) {
		for (uint8_t i = 0; i < led_buffer.count && cbuf_empty(hcbuf_modbus); i++) {
			if (i == lednum || (i == lednum - 1 && lednum > led_buffer.count - 1))
				color_holder.brightness = LED_BRIGHTNESS_HIGH;
			else if (i == lednum - 1)
				color_holder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
			else if (i == lednum - 2)
				color_holder.brightness = LED_BRIGHTNESS_MEDIUM_LOW;
			else
				color_holder.brightness = LED_BRIGHTNESS_LOW;

			led_set_color(i, color_holder);
		}
	} else {
		for (uint8_t i = 0; i < led_buffer.count && cbuf_empty(hcbuf_modbus); i++) {
			if (i == lednum || (i == lednum - 1 && lednum > led_buffer.count - 1))
				color_holder.brightness = LED_BRIGHTNESS_HIGH;
			else if (i == lednum - 1)
				color_holder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
			else if (i == lednum - 2)
				color_holder.brightness = LED_BRIGHTNESS_MEDIUM_LOW;
			else
				color_holder.brightness = LED_BRIGHTNESS_LOW;

			led_set_color(i, color_holder);
		}
	}
	led_transmit();
}

void led_knight_rider(void) {
	static kr_dir_t dir = kr_dir_right;
	while (cbuf_empty(hcbuf_modbus)) {
		for (uint8_t i = 0; i < led_buffer.count + 2 && cbuf_empty(hcbuf_modbus); i++) {
			kr_run(i, dir);
			HAL_Delay(50);
		}
		dir = dir == kr_dir_right ? kr_dir_left : kr_dir_right;
	}
}

void rainbow(void) {
	for (uint8_t i = 0; i < 25 && cbuf_empty(hcbuf_modbus); i++) {
		for (uint8_t j = 0; j < 6 && cbuf_empty(hcbuf_modbus); j++) {
			for (uint8_t k = 0; k < 32 && cbuf_empty(hcbuf_modbus); k++) {
				for (uint8_t l = 0; l < led_buffer.count && cbuf_empty(hcbuf_modbus); l++) {
					argb_t led_setting;
					led_get_color(l, &led_setting.brightness, &led_setting.color.r, &led_setting.color.g, &led_setting.color.b);
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
					led_set_color(l, led_setting);
				}
				HAL_Delay(1);
				led_transmit();
			}
		}
	}
}

void fade_brighten(uint8_t position, uint8_t statement) {
	argb_t led_setting;
	led_get_color(position, &led_setting.brightness, &led_setting.color.r, &led_setting.color.g, &led_setting.color.b);
	if (statement == 0) {
		led_setting.color.b += 2;
	} else if (statement == 1) {
		led_setting.color.b++;
		led_setting.color.g++;
	} else if (statement == 2) {
		led_setting.color.r += 2;
	} else if (statement == 3) {
		led_setting.color.r++;
		led_setting.color.g++;
	} else if (statement == 4) {
		led_setting.color.g += 2;
	} else {
		led_setting.color.g++;
		led_setting.color.b++;
	}
	led_set_color(position, led_setting);
	led_transmit();
}

void fade_darken(uint8_t position, uint8_t statement) {
	argb_t led_setting;
	led_get_color(position, &led_setting.brightness, &led_setting.color.r, &led_setting.color.g, &led_setting.color.b);

	if (statement == 0) {
		led_setting.color.b -= 2;
	} else if (statement == 1) {
		led_setting.color.b--;
		led_setting.color.r--;
	} else if (statement == 2) {
		led_setting.color.r -= 2;
	} else if (statement == 3) {
		led_setting.color.r--;
		led_setting.color.g--;
	} else if (statement == 4) {
		led_setting.color.g -= 2;
	} else {
		led_setting.color.g--;
		led_setting.color.b--;
	}
	led_set_color(position, led_setting);

	led_transmit();
}

void fade(void) {
	for (uint8_t j = 0; j < 32 && cbuf_empty(hcbuf_modbus); j++) {
		for (uint8_t k = 0; k < led_buffer.count && cbuf_empty(hcbuf_modbus); k++) {
			fade_darken(k, k % 6);
		}
	}
	for (uint8_t i = 0; i < 6 && cbuf_empty(hcbuf_modbus); i++) {
		for (uint8_t j = 0; j < 32 && cbuf_empty(hcbuf_modbus); j++) {
			for (uint8_t k = 0; k < led_buffer.count && cbuf_empty(hcbuf_modbus); k++) {
				fade_brighten(k, i);
			}
		}
		for (uint8_t j = 0; j < 32 && cbuf_empty(hcbuf_modbus); j++) {
			for (uint8_t k = 0; k < led_buffer.count && cbuf_empty(hcbuf_modbus); k++) {
				fade_darken(k, i);
			}
		}
	}
}

void police(void) {
	for (uint8_t i = 0; i < 40 && cbuf_empty(hcbuf_modbus); i++) {
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
			led_set_color(j, led_setting);
		}
		led_transmit();
		HAL_Delay(200);
	}
}

void kr_color(void) {
	for (uint8_t i = 0; i < 8 && cbuf_empty(hcbuf_modbus); i++) {
		argb_t led_setting = {.brightness = LED_BRIGHTNESS_HIGH};
		for (uint8_t j = 0; j < led_buffer.count && cbuf_empty(hcbuf_modbus); j++) {
			for (uint8_t k = 0; k < led_buffer.count && cbuf_empty(hcbuf_modbus); k++) {
				if (k <= j) {
					if (i % 2) {
						led_setting.color.r = 32;
						led_setting.color.g = 32;
						led_setting.color.b = 0;
					} else {
						led_setting.color.r = 32;
						led_setting.color.g = 0;
						led_setting.color.b = 32;
					}
				}
				led_set_color(k, led_setting);
			}
			led_transmit();
			HAL_Delay(20);
		}

		for (uint8_t j = 0; j < led_buffer.count && cbuf_empty(hcbuf_modbus); j++) {
			for (uint8_t k = 0; k < led_buffer.count && cbuf_empty(hcbuf_modbus); k++) {
				if (k <= j) {
					if (i % 2) {
						led_setting.color.r = 32;
						led_setting.color.g = 32;
						led_setting.color.b = 0;
					} else {
						led_setting.color.r = 32;
						led_setting.color.g = 0;
						led_setting.color.b = 32;
					}
					led_set_color(k, led_setting);
				}
			}
		}
		led_transmit();
		HAL_Delay(20);
	}
}
