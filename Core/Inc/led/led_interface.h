#ifndef LED_INTERFACE_H
#define LED_INTERFACE_H

#include "main.h"

/* Configuration */
#define MAX_LEDS 40
#define BITS_PER_LED 24
#define MAX_MODULES_COUNT MAX_LEDS / 8

/* Error codes */
typedef enum {
	LED_OK = 0,
	LED_ERR_INVALID_INDEX,
	LED_ERR_BUSY,
	LED_ERR_NULL_POINTER
} led_err_t;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t;

extern const rgb_t led_red;
extern const rgb_t led_green;
extern const rgb_t led_blue;
extern const rgb_t led_white;
extern const rgb_t led_yellow;
extern const rgb_t led_cyan;
extern const rgb_t led_magenta;
extern const rgb_t led_orange;
extern const rgb_t led_black;

typedef enum led_brightness {
	LED_BRIGHTNESS_LOW,
	LED_BRIGHTNESS_MEDIUM_LOW,
	LED_BRIGHTNESS_MEDIUM,
	LED_BRIGHTNESS_MEDIUM_HIGH,
	LED_BRIGHTNESS_HIGH
} led_brightness_t;

typedef struct led {
	led_brightness_t brightness;
	rgb_t color;
} argb_t;

typedef struct {
	argb_t leds[MAX_LEDS];
	uint8_t count;
	uint8_t busy;
} led_buffer_t;

extern led_buffer_t led_buffer;

/**
 * Initialize LED controller
 */
void led_init(void);

/**
 * Set LED color (non-blocking queue)
 * @param index LED index (0 to MAX_LEDS-1)
 * @param rgb_t struct RGB color values
 * @return LED_OK if success, error code otherwise
 */
led_err_t led_set_color(uint8_t index, rgb_t led_settings);

/**
 * Set LED color with brightness (non-blocking queue)
 * @param index LED index (0 to MAX_LEDS-1)
 * @param argb_t struct containing brightness and RGB color values
 * @return LED_OK if success, error code otherwise
 */
led_err_t led_set_colorWithBrightness(uint8_t index, argb_t led_settings);

/**
 * Get current LED color
 * @param index LED index
 * @param a Pointer to alpha/brightness value (optional, can be NULL)
 * @param r Pointer to red value
 * @param g Pointer to green value
 * @param b Pointer to blue value
 * @return LED_OK if success, error code otherwise
 */
led_err_t led_get_color(uint8_t index, uint8_t *a, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * Get LED count
 */
uint8_t led_get_count(void);

/**
 * Check if transmission is busy
 */
uint8_t led_is_busy(void);

/**
 * Start LED data transmission (blocking)
 * @return LED_OK if transmission completed, error code otherwise
 */
led_err_t led_transmit(void);

/**
 * update the current led mode
 */
void led_updateMode(void);

#endif /* LED_INTERFACE_H */