#include "../../Inc/led/led_interface.h"
#include "../../Inc/led/led_modes.h"
#include "cmsis_gcc.h"
#include "modbus/circularBuffer.h"
#include "modbus/modbus_config.h"
#include "modbus/modbus_interface.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_tim.h"
#include "tim.h"

#define led_low() (LED_GPIO_Port->BRR = LED_Pin)
#define led_high() (LED_GPIO_Port->BSRR = LED_Pin)

led_buffer_t led_buffer;

__STATIC_FORCEINLINE void delay_short(uint32_t ns);
__STATIC_FORCEINLINE void send_bit(uint8_t bit);
__STATIC_FORCEINLINE void send_byte(uint8_t byte);

led_mode_fn_t mode_off = &led_turnOff;
led_mode_fn_t mode_normal = &led_turnOn;
led_mode_fn_t mode_vegas = &led_vegas;
led_mode_fn_t mode_knightrider = &led_knight_rider;
led_mode_fn_t mode_bootup = &led_bootup;

const rgb_t led_red = {255, 0, 0};
const rgb_t led_green = {0, 255, 0};
const rgb_t led_blue = {0, 0, 255};
const rgb_t led_white = {255, 255, 255};
const rgb_t led_yellow = {255, 255, 0};
const rgb_t led_cyan = {0, 255, 255};
const rgb_t led_magenta = {255, 0, 255};
const rgb_t led_orange = {255, 165, 0};
const rgb_t led_black = {0, 0, 0};

/**
 * Initialize LED controller
 */
void led_init(void) {
	HAL_TIM_Base_Start(&htim1);
	led_buffer.busy = 0;
	led_low();
	// Set all leds to self test pattern (alternating off / on) on startup
	for (uint8_t i = 0; i < hmb->inputRegisters[0]; i++) {
		if (i & 0x01) {
			led_set_colorWithBrightness(i, (argb_t){.color = led_black, .brightness = LED_BRIGHTNESS_LOW});
		} else {
			led_set_colorWithBrightness(i, (argb_t){.color = led_white, .brightness = LED_BRIGHTNESS_HIGH});
		}
	}
	led_transmit();
	hmb->ledMode = LED_MODE_BOOTUP;
	led_updateMode();
	LOG_INFO("LED controller initialized, resetting all LEDs to off");
	LOG_DEBUG("LED init complete: count=%u", led_buffer.count);
	LOG_DEBUG("LED buffer state: busy=%u", led_buffer.busy);
#ifdef LOG_LEVEL_VERBOSE
	LOG_VERBOSE("LED buffer contents:");
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		LOG_VERBOSE("LED %u: R=%u G=%u B=%u Brightness=%u", i, led_buffer.leds[i].color.r, led_buffer.leds[i].color.g, led_buffer.leds[i].color.b, led_buffer.leds[i].brightness);
	}
#endif
}

/**
 * Set LED color
 */
led_err_t led_set_color(uint8_t index, rgb_t led_settings) {
	if (index >= MAX_LEDS) {
		return LED_ERR_INVALID_INDEX;
	}

	if (led_buffer.busy) {
		return LED_ERR_BUSY;
	}

	led_buffer.leds[index].color.r = led_settings.r;
	led_buffer.leds[index].color.g = led_settings.g;
	led_buffer.leds[index].color.b = led_settings.b;

	if (index >= led_buffer.count) {
		led_buffer.count = index + 1;
	}

	return LED_OK;
}

/**
 * Set LED color
 */
led_err_t led_set_colorWithBrightness(uint8_t index, argb_t led_settings) {
	if (index >= MAX_LEDS) {
		return LED_ERR_INVALID_INDEX;
	}

	if (led_buffer.busy) {
		return LED_ERR_BUSY;
	}

	led_buffer.leds[index].brightness = led_settings.brightness;

	if (led_settings.brightness == LED_BRIGHTNESS_LOW) {
		led_buffer.leds[index].color.r = 0;
		led_buffer.leds[index].color.g = 0;
		led_buffer.leds[index].color.b = 0;
	} else if (led_settings.brightness == LED_BRIGHTNESS_MEDIUM_LOW) {
		led_buffer.leds[index].color.r = led_settings.color.r / 3;
		led_buffer.leds[index].color.g = led_settings.color.g / 3;
		led_buffer.leds[index].color.b = led_settings.color.b / 3;

	} else if (led_settings.brightness == LED_BRIGHTNESS_MEDIUM) {
		led_buffer.leds[index].color.r = led_settings.color.r / 2;
		led_buffer.leds[index].color.g = led_settings.color.g / 2;
		led_buffer.leds[index].color.b = led_settings.color.b / 2;

	} else if (led_settings.brightness == LED_BRIGHTNESS_MEDIUM_HIGH) {
		led_buffer.leds[index].color.r = led_settings.color.r / 4 * 3;
		led_buffer.leds[index].color.g = led_settings.color.g / 4 * 3;
		led_buffer.leds[index].color.b = led_settings.color.b / 4 * 3;
	} else {
		led_buffer.leds[index].color.r = led_settings.color.r;
		led_buffer.leds[index].color.g = led_settings.color.g;
		led_buffer.leds[index].color.b = led_settings.color.b;
	}

	if (index >= led_buffer.count) {
		led_buffer.count = index + 1;
	}

	return LED_OK;
}

/**
 * Get current LED color
 */
led_err_t led_get_color(uint8_t index, uint8_t *a, uint8_t *r, uint8_t *g, uint8_t *b) {
	if (index >= MAX_LEDS) {
		return LED_ERR_INVALID_INDEX;
	}

	if (r == NULL || g == NULL || b == NULL) {
		return LED_ERR_NULL_POINTER;
	}

	*r = led_buffer.leds[index].color.r;
	*g = led_buffer.leds[index].color.g;
	*b = led_buffer.leds[index].color.b;

	if (a != NULL) {
		*a = led_buffer.leds[index].brightness;
	}

	return LED_OK;
}

/**
 * Get LED count
 */
uint8_t led_get_count(void) {
	return led_buffer.count;
}

/**
 * Check if transmission is busy
 */
uint8_t led_is_busy(void) {
	return led_buffer.busy;
}

/**
 * Start LED data transmission
 * Duration: ~30us per LED + 50us reset
 * For 40 LEDs: ~1.25ms total
 */
led_err_t led_transmit(void) {
	led_buffer.busy = 1;
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		send_byte(led_buffer.leds[i].color.g); /* Green */
		send_byte(led_buffer.leds[i].color.r); /* Red */
		send_byte(led_buffer.leds[i].color.b); /* Blue */
	}

	delay_short(50000); /* Reset pulse >50us */

	led_buffer.busy = 0;

	return LED_OK;
}

void led_updateMode(void) {
	static uint8_t lastMode = 0xFF;
	if (hmb->ledMode != lastMode) {
		LOG_DEBUG("Updating LED mode: %u", hmb->ledMode);
	}
	lastMode = hmb->ledMode;
	hmb->holdingRegisters[41] = hmb->ledMode;
	switch (hmb->ledMode) {
		case LED_MODE_NORMAL:
			mode_normal();
			break;
		case LED_MODE_VEGAS:
			mode_vegas();
			break;
		case LED_MODE_KNIGHT:
			mode_knightrider();
			break;
		case LED_MODE_BOOTUP:
			mode_bootup();
			break;
		default:
			mode_off();
			break;
	}

	led_transmit();
}

__STATIC_FORCEINLINE void delay_short(uint32_t ns) {
	ns = (ns * 48) / 1000;   /* Convert ns to timer ticks at 48MHz */
	htim1.Instance->CNT = 0; /* Reset timer counter */
	while (htim1.Instance->CNT < ns)
		;
}

/**
 * Send one bit to WS2812B
 * T0H: 0.4us (19 cycles @ 48MHz)
 * T0L: 0.85us (41 cycles @ 48MHz)
 * T1H: 0.8us (38 cycles @ 48MHz)
 * T1L: 0.45us (22 cycles @ 48MHz)
 */
__STATIC_FORCEINLINE void send_bit(uint8_t bit) {
	if (bit) {
		/* Send '1' bit */
		led_high();
		delay_short(234);
		led_low();
		delay_short(0);
	} else {
		/* Send '0' bit */
		led_high();
		delay_short(0);
		led_low();
		delay_short(234);
	}
}

/**
 * Send one byte to WS2812B
 */
__STATIC_FORCEINLINE void send_byte(uint8_t byte) {
	for (int8_t i = 7; i >= 0; i--) {
		send_bit((byte >> i) & 1);
	}
}