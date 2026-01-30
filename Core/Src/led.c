#include "led.h"
#include "modbus/modbus_interface.h"
#include <stdint.h>
#include <string.h>

#define led_low() (LED_GPIO_Port->BRR = LED_Pin)
#define led_high() (LED_GPIO_Port->BSRR = LED_Pin)

typedef struct {
  rgb_t leds[MAX_LEDS];
  uint8_t count;
  uint8_t busy;
} led_buffer_t;

__STATIC_FORCEINLINE void send_bit(uint8_t bit);
__STATIC_FORCEINLINE void send_byte(uint8_t byte);

static led_buffer_t led_buffer = {0};

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
led_err_t led_set_color(uint8_t index, rgb_t color) {
  if (index >= MAX_LEDS) {
    return LED_ERR_INVALID_INDEX;
  }

  if (led_buffer.busy) {
    return LED_ERR_BUSY;
  }

  led_buffer.leds[index].r = color.r;
  led_buffer.leds[index].g = color.g;
  led_buffer.leds[index].b = color.b;

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
  delay_250ns(200); /* 50us @ 48MHz */
  led_buffer.busy = 0;
  return LED_OK;
}

void led_updateMode(void) {
  // TODO: implement other led modes

  for (uint8_t i = 0; i < led_buffer.count; i++) {
    if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_ERROR_FLAG)
      led_set_color(i, led_red);
    else if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_NEWOP_FLAG)
      led_set_color(i, led_blue);
    else if (hmb->holdingRegisters[i] & HOLDINGREG_SLOT_TAKEN_FLAG)
      led_set_color(i, led_green);
    else
      led_set_color(i, led_black);
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