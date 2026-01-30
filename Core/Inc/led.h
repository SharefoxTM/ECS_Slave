#ifndef LED_H
#define LED_H

#include "main.h"
#include <stdint.h>

/* Configuration */
#define MAX_LEDS 40
#define BITS_PER_LED 24

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
void led_init(void);

/**
 * Set LED color (non-blocking queue)
 * @param index LED index (0 to MAX_LEDS-1)
 * @param r Red value (0-255)
 * @param g Green value (0-255)
 * @param b Blue value (0-255)
 * @return LED_OK if success, error code otherwise
 */
led_err_t led_set_color(uint8_t index, rgb_t color);

/**
 * Get current LED color
 * @param index LED index
 * @param r Pointer to red value
 * @param g Pointer to green value
 * @param b Pointer to blue value
 * @return LED_OK if success, error code otherwise
 */
led_err_t led_get_color(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);

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

#endif /* LED_H */