#include "shift_register.h"
#include "cmsis_gcc.h"
#include "main.h"
#include "stm32f030x8.h"
#include "stm32f0xx.h"
#include <string.h>

#define cycleLatch() cyclePinHigh(SR_Latch_GPIO_Port, SR_Latch_Pin)
#define cycleClock() cyclePinLow(SR_CLK_GPIO_Port, SR_CLK_Pin)
#define readBit() ((SR_SDI_GPIO_Port->IDR & SR_SDI_Pin) != 0)

// Hardware abstraction layer functions (implement these for your STM32)
void ParallelLoad(void);
__STATIC_FORCEINLINE void cyclePinHigh(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
__STATIC_FORCEINLINE void cyclePinLow(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

ShiftRegister_t ShiftRegister_Init(void) {
  ShiftRegister_t sr;
  memset(sr.sensorStates, 0, sizeof(sr.sensorStates));
  sr.activeRegisters = 0;
  sr.totalSlots = 0;
  ShiftRegister_DetectSlots(&sr);
  ShiftRegister_ReadSensors(&sr);
  return sr;
}

uint8_t ShiftRegister_DetectSlots(ShiftRegister_t *sr) {
  SR_SDO_GPIO_Port->BRR = SR_SDO_Pin;
  SR_Latch_GPIO_Port->BSRR = SR_Latch_Pin;

  sr->totalSlots = 0;

  for (uint8_t i = 0; i < MAX_SHIFT_REGISTERS * BITS_PER_SR; i++) {
    cycleClock();
  }

  SR_SDO_GPIO_Port->BSRR = SR_SDO_Pin;

  while ((SR_SDI_GPIO_Port->IDR & SR_SDI_Pin) == GPIO_PIN_RESET) {
    sr->totalSlots += 1;

    cycleClock();
  }
  sr->activeRegisters = (sr->totalSlots + BITS_PER_SR - 1) / BITS_PER_SR;

  return sr->totalSlots;
}

void ShiftRegister_ReadSensors(ShiftRegister_t *sr) {
  if (sr->activeRegisters == 0) {
    return;
  }

  cycleLatch();

  // Read only active shift registers
  for (int8_t srIndex = sr->activeRegisters - 1; srIndex >= 0; srIndex--) {
    uint8_t data = 0;

    for (int8_t bit = 7; bit >= 0; bit--) {
      data |= (readBit() << bit);
      cycleClock();
    }

    sr->sensorStates[srIndex] = data;
  }
}

bool ShiftRegister_GetSlotState(ShiftRegister_t *sr, uint8_t slot) {
  if (slot >= sr->totalSlots) {
    return RESET;
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

__STATIC_FORCEINLINE void cyclePinHigh(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
  GPIOx->BSRR = GPIO_Pin;
  delay_250ns(2);
  GPIOx->BRR = GPIO_Pin;
  delay_250ns(2);
}
__STATIC_FORCEINLINE void cyclePinLow(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
  GPIOx->BRR = GPIO_Pin;
  delay_250ns(2);
  GPIOx->BSRR = GPIO_Pin;
  delay_250ns(2);
}