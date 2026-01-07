#include "led.h"
#include "main.h"
#include <string.h>

/* LED Buffer */
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb_t;

typedef struct {
  rgb_t leds[MAX_LEDS];
  uint8_t count;
  uint8_t busy;
} led_buffer_t;

static led_buffer_t led_buffer = {0};

/* Inline GPIO functions for speed */
__STATIC_FORCEINLINE void led_high(void) { LED_GPIO_Port->BSRR = LED_Pin; }

__STATIC_FORCEINLINE void led_low(void) { LED_GPIO_Port->BRR = LED_Pin; }

/**
 * Delay in CPU cycles (at 48MHz)
 * @param cycles Number of cycles to delay
 */
__STATIC_FORCEINLINE void delay_cycles(uint32_t cycles) {
  /* Each iteration takes ~4 cycles */
  cycles = cycles / 4;
  while (cycles--) {
    __NOP();
  }
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
    delay_cycles(34); /* T1H: ~0.7us (slightly shorter for overhead) */
    led_low();
    delay_cycles(18); /* T1L: ~0.4us */
  } else {
    /* Send '0' bit */
    led_high();
    delay_cycles(15); /* T0H: ~0.3us (slightly shorter for overhead) */
    led_low();
    delay_cycles(37); /* T0L: ~0.8us */
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

/**
 * Initialize LED controller
 */
void led_init(void) {
  memset(&led_buffer, 0, sizeof(led_buffer));
  led_buffer.busy = 0;
  led_low();
}

/**
 * Set LED color (non-blocking queue)
 */
led_err_t led_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index >= MAX_LEDS) {
    return LED_ERR_INVALID_INDEX;
  }

  if (led_buffer.busy) {
    return LED_ERR_BUSY;
  }

  led_buffer.leds[index].r = r;
  led_buffer.leds[index].g = g;
  led_buffer.leds[index].b = b;

  if (index >= led_buffer.count) {
    led_buffer.count = index + 1;
  }

  return LED_OK;
}

/**
 * Get current LED color
 */
led_err_t led_get_color(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (index >= MAX_LEDS) {
    return LED_ERR_INVALID_INDEX;
  }

  if (r == NULL || g == NULL || b == NULL) {
    return LED_ERR_NULL_POINTER;
  }

  *r = led_buffer.leds[index].r;
  *g = led_buffer.leds[index].g;
  *b = led_buffer.leds[index].b;

  return LED_OK;
}

/**
 * Get LED count
 */
uint8_t led_get_count(void) { return led_buffer.count; }

/**
 * Check if transmission is busy
 */
uint8_t led_is_busy(void) { return led_buffer.busy; }

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
    send_byte(led_buffer.leds[i].g); /* Green */
    send_byte(led_buffer.leds[i].r); /* Red */
    send_byte(led_buffer.leds[i].b); /* Blue */
  }

  /* Re-enable interrupts */
  __enable_irq();

  /* Reset period: >50us low */
  led_low();
  delay_cycles(2400); /* 50us @ 48MHz */

  led_buffer.busy = 0;
  return LED_OK;
}