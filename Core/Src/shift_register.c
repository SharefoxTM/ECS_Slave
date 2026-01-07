#include "shift_register.h"
#include <string.h>

// Hardware abstraction layer functions (implement these for your STM32)
extern void SR_SetClock(bool state);
extern void SR_SetLatch(bool state);
extern void SR_SetLoad(bool state);
extern bool SR_ReadData(void);
extern void SR_DelayUs(uint32_t us);

void ShiftRegister_Init(ShiftRegister_t *sr) {
  memset(sr->sensorStates, 0, sizeof(sr->sensorStates));
  sr->activeRegisters = 0;
  sr->totalSlots = 0;
}

static void ShiftRegister_LoadParallel(void) {
  // Pulse load pin to capture parallel inputs from shift registers
  SR_SetLoad(false);
  SR_DelayUs(5);
  SR_SetLoad(true);
  SR_DelayUs(5);
}

uint8_t ShiftRegister_DetectSlots(ShiftRegister_t *sr) {
  // Detect number of shift registers by reading until no more data
  // Assumes shift registers are daisy-chained
  sr->activeRegisters = 0;
  ShiftRegister_LoadParallel();

  for (uint8_t srIndex = 0; srIndex < MAX_SHIFT_REGISTERS; srIndex++) {
    uint8_t data = 0;

    // Read 8 bits
    for (int8_t bit = 7; bit >= 0; bit--) {
      data |= (SR_ReadData() << bit);

      // Pulse clock
      SR_SetClock(true);
      SR_DelayUs(5);
      SR_SetClock(false);
      SR_DelayUs(5);
    }

    sr->sensorStates[srIndex] = data;
    sr->activeRegisters++;

    // If this register reads all zeros and next would too, stop detection
    if (data == 0 && srIndex > 0) {
      // Continue to detect full chain - don't break on zero
    }
  }

  sr->totalSlots = sr->activeRegisters * BITS_PER_SR;

  return sr->totalSlots;
}

void ShiftRegister_ReadSensors(ShiftRegister_t *sr) {
  if (sr->activeRegisters == 0) {
    return;
  }

  ShiftRegister_LoadParallel();

  // Read only active shift registers
  for (int8_t srIndex = sr->activeRegisters - 1; srIndex >= 0; srIndex--) {
    uint8_t data = 0;

    for (int8_t bit = 7; bit >= 0; bit--) {
      data |= (SR_ReadData() << bit);

      // Pulse clock to shift next bit
      SR_SetClock(true);
      SR_DelayUs(5);
      SR_SetClock(false);
      SR_DelayUs(5);
    }

    sr->sensorStates[srIndex] = data;
  }
}

bool ShiftRegister_GetSlotState(ShiftRegister_t *sr, uint8_t slot) {
  if (slot >= sr->totalSlots) {
    return false;
  }

  uint8_t srIndex = slot / BITS_PER_SR;
  uint8_t bitIndex = slot % BITS_PER_SR;
  uint8_t bitMask = (1 << bitIndex);

  // Return actual sensor state
  return (sr->sensorStates[srIndex] & bitMask) != 0;
}

uint8_t ShiftRegister_GetOccupiedCount(ShiftRegister_t *sr) {
  uint8_t count = 0;

  for (uint8_t slot = 0; slot < sr->totalSlots; slot++) {
    if (ShiftRegister_GetSlotState(sr, slot)) {
      count++;
    }
  }

  return count;
}

uint8_t ShiftRegister_GetFreeCount(ShiftRegister_t *sr) {
  return sr->totalSlots - ShiftRegister_GetOccupiedCount(sr);
}

uint8_t ShiftRegister_GetTotalSlots(ShiftRegister_t *sr) {
  return sr->totalSlots;
}