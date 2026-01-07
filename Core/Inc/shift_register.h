#ifndef SHIFT_REGISTER_H
#define SHIFT_REGISTER_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_SHIFT_REGISTERS 5
#define BITS_PER_SR 8

typedef struct {
  uint8_t
      sensorStates[MAX_SHIFT_REGISTERS]; // Raw sensor data from Hall sensors
  uint8_t activeRegisters;               // Number of active shift registers
  uint8_t totalSlots;                    // Total number of slots
} ShiftRegister_t;

// Function prototypes
void ShiftRegister_Init(ShiftRegister_t *sr);
uint8_t ShiftRegister_DetectSlots(ShiftRegister_t *sr);
void ShiftRegister_ReadSensors(ShiftRegister_t *sr);
bool ShiftRegister_GetSlotState(ShiftRegister_t *sr, uint8_t slot);
void ShiftRegister_SetOverride(ShiftRegister_t *sr, uint8_t slot, bool state);
bool ShiftRegister_GetOverride(ShiftRegister_t *sr, uint8_t slot);
uint8_t ShiftRegister_GetOccupiedCount(ShiftRegister_t *sr);
uint8_t ShiftRegister_GetFreeCount(ShiftRegister_t *sr);
uint8_t ShiftRegister_GetTotalSlots(ShiftRegister_t *sr);

#endif // SHIFT_REGISTER_H