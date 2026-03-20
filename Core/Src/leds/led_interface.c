#include "../../Inc/led/led_interface.h"
#include "../../Inc/led/led_modes.h"

#define led_low() (LED_GPIO_Port->BRR = LED_Pin)
#define led_high() (LED_GPIO_Port->BSRR = LED_Pin)

led_buffer_t led_buffer;

__STATIC_FORCEINLINE void send_bit(uint8_t bit);
__STATIC_FORCEINLINE void send_byte(uint8_t byte);

led_mode_fn_t mode_off = &led_turnOff;
led_mode_fn_t mode_normal = &led_turnOn;
led_mode_fn_t mode_vegas = &led_vegas;
led_mode_fn_t mode_knightrider = &led_knight_rider;

rgb_t led_red = {255, 0, 0};
rgb_t led_green = {0, 255, 0};
rgb_t led_blue = {0, 0, 255};
rgb_t led_white = {255, 255, 255};
rgb_t led_yellow = {255, 255, 0};
rgb_t led_cyan = {0, 255, 255};
rgb_t led_magenta = {255, 0, 255};
rgb_t led_black = {0, 0, 0};

/**
 * Initialize LED controller
 */
void led_init(void) {
	led_buffer.busy = 0;
	led_low();
}

/**
 * Set LED color (non-blocking queue)
 */
led_err_t led_set_color(uint8_t index, argb_t led_settings) {
	if (index >= MAX_LEDS) {
		return LED_ERR_INVALID_INDEX;
	}

	if (led_buffer.busy) {
		return LED_ERR_BUSY;
	}

	led_buffer.leds[index].color.r = led_settings.color.r;
	led_buffer.leds[index].color.g = led_settings.color.g;
	led_buffer.leds[index].color.b = led_settings.color.b;

	led_buffer.leds[index].brightness = led_settings.brightness;

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
 * Start LED data transmission (blocking but fast)
 * Duration: ~30us per LED + 50us reset
 * For 40 LEDs: ~1.25ms total
 */
led_err_t led_transmit(void) {
	if (led_buffer.busy) {
		return LED_ERR_BUSY;
	}

	led_buffer.busy = 1;

	/* Disable interrupts for precise timing */
	__disable_irq();

	/* Send LED data in GRB format */
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		send_byte(led_buffer.leds[i].color.g); /* Green */
		send_byte(led_buffer.leds[i].color.r); /* Red */
		send_byte(led_buffer.leds[i].color.b); /* Blue */
	}

	/* Re-enable interrupts */
	__enable_irq();

	/* Reset period: >50us low */
	led_low();
	delay_250ns(200); /* 50us @ 48MHz */
	led_buffer.busy = 0;
	return LED_OK;
}

void led_updateMode(void) {
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
		default:
			mode_off();
			break;
	}

	led_transmit();
}

/**
 * Send one bit to WS2812B using bit-banging
 * T0H: 0.4us (19 cycles @ 48MHz)
 * T0L: 0.85us (41 cycles @ 48MHz)
 * T1H: 0.8us (38 cycles @ 48MHz)
 * T1L: 0.45us (22 cycles @ 48MHz)
 */
__STATIC_FORCEINLINE void send_bit(uint8_t bit) {
	if (bit) {
		/* Send '1' bit */
		led_high();
		delay_250ns(4); /* T1H: 1us */
		led_low();
		delay_250ns(1); /* T1L: 0.25us */
	} else {
		/* Send '0' bit */
		led_high();
		delay_250ns(1); /* T0H: 0.25us */
		led_low();
		delay_250ns(4); /* T0L: 1us */
	}
}

/**
 * Send one byte to WS2812B (MSB first)
 */
__STATIC_FORCEINLINE void send_byte(uint8_t byte) {
	for (int8_t i = 7; i >= 0; i--) {
		send_bit((byte >> i) & 1);
	}
}