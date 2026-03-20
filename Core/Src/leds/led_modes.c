#include "led/led_modes.h"
#include "led/led_interface.h"

void rainbow(void);
void fade(void);
void police(void);
void kr_color(void);

led_mode_fn_t vegasShow[4] = { &rainbow, &fade, &police, &kr_color };

typedef enum {
    kr_dir_left,
    kr_dir_right
} kr_dir_t;

void led_turnOff(void) {
	for (uint8_t i = 0; i < led_buffer.count; i++) {
		led_set_color(i, led_black);
	}
}

void led_turnOn(void) {
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
}

void led_vegas(void) {
    while (!get_newDataFlag()) {
        for (int i = 0; i < led_buffer.count && !get_newDataFlag(); i++) {
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
            led_buffer.leds[i].brightness = LED_BRIGHTNESS_HIGH;
            led_buffer.leds[i].color = color_holder;
        }
        update_leds();
        for (int i = 0; i < 4 && !get_newDataFlag(); i++) {
            vegasShow[i]();
        }
    }
}

void led_knight_rider(void) {
    static kr_dir_t dir = kr_dir_right;
    while (!get_newDataFlag()) {
        for (uint8_t i = 0; i < led_buffer.count + 2 && !get_newDataFlag(); i++) {
            kr_run(i, dir);
            HAL_Delay(50);
        }
        dir = dir == kr_dir_right ? kr_dir_left : kr_dir_right;
    }
}

void kr_run(uint8_t lednum, kr_dir_t dir) {
    if (dir == kr_dir_right) {
        for (uint8_t i = 0; i < led_buffer.count && !get_newDataFlag(); i++) {
                led_buffer.leds[i].color = led_red;
                if (i == lednum || (i == lednum - 1 && lednum > led_buffer.count - 1))
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_HIGH;
                else if (i == lednum - 1)
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
                else if (i == lednum - 2)
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_MEDIUM_LOW;
                else
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_LOW;
        }
    } else {
        for (uint8_t i = 0; i < led_buffer.count && !get_newDataFlag(); i++) {
            led_buffer.leds[i].color = led_red;
            if (i == lednum || (i == lednum - 1 && lednum > led_buffer.count - 1))
                led_buffer.leds[i].brightness = LED_BRIGHTNESS_HIGH;
            else if (i == lednum - 1)
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
                else if (i == lednum - 2)
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_MEDIUM_LOW;
                else
                    led_buffer.leds[i].brightness = LED_BRIGHTNESS_LOW;
            }
    }
    update_leds();
}

void rainbow(void) {
    for (uint8_t i = 0; i < 25 && !get_newDataFlag(); i++) {
        for (uint8_t j = 0; j < 6 && !get_newDataFlag(); j++) {
            for (uint8_t k = 0; k < 32 && !get_newDataFlag(); k++) {
                for (uint8_t l = 0; l < led_buffer.count && !get_newDataFlag(); l++) {
                    if ((j + l) % 6 == 0) {
                        led_buffer.leds[l].color.r++;
                        led_buffer.leds[l].color.b--;
                    } else if ((j + l) % 6 == 1) {
                        led_buffer.leds[l].color.r++;
                        led_buffer.leds[l].color.b--;
                    } else if ((j + l) % 6 == 2) {
                        led_buffer.leds[l].color.g++;
                        led_buffer.leds[l].color.r--;
                    } else if ((j + l) % 6 == 3) {
                        led_buffer.leds[l].color.g++;
                        led_buffer.leds[l].color.r--;
                    } else if ((j + l) % 6 == 4) {
                        led_buffer.leds[l].color.b++;
                        led_buffer.leds[l].color.g--;
                    } else {
                        led_buffer.leds[l].color.b++;
                        led_buffer.leds[l].color.g--;
                    }
                }
                HAL_Delay(1);
                update_leds();
            }
        }
    }
}

void fade_brighten(uint8_t position, uint8_t statement) {
    if (statement == 0) {
        led_buffer.leds[position].color.b += 2;
    } else if (statement == 1) {
        led_buffer.leds[position].color.b++;
        led_buffer.leds[position].color.g++ ;
    } else if (statement == 2) {
        led_buffer.leds[position].color.r += 2;
    } else if (statement == 3) {
        led_buffer.leds[position].color.r++;
        led_buffer.leds[position].color.g++;
    } else if (statement == 4) {
        led_buffer.leds[position].color.g += 2;
    } else {
        led_buffer.leds[position].color.g++;
        led_buffer.leds[position].color.b++;
    }
    update_leds();
}

void fade_darken(uint8_t position, uint8_t statement)
{
    if (statement == 0) {
        led_buffer.leds[position].color.b -= 2;
    } else if (statement == 1) {
        led_buffer.leds[position].color.b--;
        led_buffer.leds[position].color.r--;
    } else if (statement == 2) {
        led_buffer.leds[position].color.r -= 2;
    } else if (statement == 3) {
        led_buffer.leds[position].color.r--;
        led_buffer.leds[position].color.g--;
    } else if (statement == 4) {
        led_buffer.leds[position].color.g -= 2;
    } else {
        led_buffer.leds[position].color.g--;
        led_buffer.leds[position].color.b--;
    }

    update_leds();
}

void fade(void)
{
    for (uint8_t j = 0; j < 32 && !get_newDataFlag(); j++) {
        for (uint8_t k = 0; k < led_buffer.count && !get_newDataFlag(); k++) {
            fade_darken(k, k % 6);
        }
    }
    for (uint8_t i = 0; i < 6 && !get_newDataFlag(); i++) {
        for (uint8_t j = 0; j < 32 && !get_newDataFlag(); j++) {
            for (uint8_t k = 0; k < led_buffer.count && !get_newDataFlag(); k++) {
                fade_brighten(k, i);
            }
        }
        for (uint8_t j = 0; j < 32 && !get_newDataFlag(); j++) {
            for (uint8_t k = 0; k < led_buffer.count && !get_newDataFlag(); k++) {
                fade_darken(k, i);
            }
        }
    }
}

void police(void)
{
    for (uint8_t i = 0; i < 40 && !get_newDataFlag(); i++) {
        for (uint8_t j = 0; j < led_buffer.count; j++) {
            if (j < led_buffer.count / 2) {
                if (i % 2)
                    led_buffer.leds[j].color = led_blue;
                else
                    led_buffer.leds[j].color = led_red;
            } else {
                if (i % 2)
                    led_buffer.leds[j].color = led_red;
                else
                    led_buffer.leds[j].color = led_blue;
            }
            led_buffer.leds[j].brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
        }
        update_leds();
        HAL_Delay(200);
    }
}

void kr_color(void)
{
    for (uint8_t i = 0; i < 8 && !get_newDataFlag(); i++) {
        for (uint8_t j = 0; j < led_buffer.count && !get_newDataFlag(); j++) {
            for (uint8_t k = 0; k < led_buffer.count && !get_newDataFlag(); k++) {
                if (k <= j) {
                    if (i % 2) {
                        led_buffer.leds[k].color.r = 32;
                        led_buffer.leds[k].color.g = 32;
                        led_buffer.leds[k].color.b = 0;
                    } else {
                        led_buffer.leds[k].color.r = 32;
                        led_buffer.leds[k].color.g = 0;
                        led_buffer.leds[k].color.b = 32;
                    }
                }
                led_buffer.leds[k].brightness = LED_BRIGHTNESS_HIGH;
            }
            update_leds();
            HAL_Delay(20);
        }

        for (uint8_t j = 0; j < led_buffer.count && !get_newDataFlag(); j++) {
            for (uint8_t k = 0; k < led_buffer.count && !get_newDataFlag(); k++) {
                if (k <= j) {
                    if (i % 2) {
                        led_buffer.leds[k].color.r = 32;
                        led_buffer.leds[k].color.g = 32;
                        led_buffer.leds[k].color.b = 0;
                    } else {
                        led_buffer.leds[k].color.r = 32;
                        led_buffer.leds[k].color.g = 0;
                        led_buffer.leds[k].color.b = 32;
                    }
                }
            }
        }
        update_leds();
        HAL_Delay(20);
    }
}
