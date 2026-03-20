#ifndef LED_MODES_H
#define LED_MODES_H

#include "led_interface.h"
#include "modbus/modbus_interface.h"

typedef void (*led_mode_fn_t)(void);

extern void led_turnOff(void);
extern void led_turnOn(void);
extern void led_vegas(void);
extern void led_knight_rider(void);

#endif /* LED_MODES_H */