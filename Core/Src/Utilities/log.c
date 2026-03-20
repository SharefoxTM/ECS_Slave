/*
 * log.c
 *
 *  Created on: Nov 7, 2025
 *      Author: wda
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#define LOG_LEVEL_VERBOSE
#include "Utilities/log.h"

extern UART_HandleTypeDef huart2;
/**
 * @brief used as an example to print the different LOG_* macro's
 *
 * Other than the example, there is no real need to call this function from
 * main.
 *
 */
void logInit() {
  // print the usage of the log macro's
  LOG_ERROR("Critical errors, software module can not recover on its own");
  LOG_WARN("Error conditions from which recovery measures have been taken");
  LOG_INFO("Information messages which describe normal flow of events");
  LOG_DEBUG("Extra information which is not necessary for normal use");
  LOG_VERBOSE("Bigger chunks of debugging information, or frequent messages "
              "which can potentially flood the output.");
}

/**
 * @brief Used by the LOG_* macro's for removing the path from __FILE__
 *
 * No need to call it directly use the macro instead.
 *
 */
char *logGetFileName(char *file) {
  return strrchr(file, '/') ? strrchr(file, '/') + 1 : file;
}

/**
 * @brief Used by the LOG_* macro's for the time stamp in ms
 *
 * No need to call it directly use the macro instead.
 *
 */
long logGetTimestamp(void) { return (long)HAL_GetTick(); }

/**
 * @brief Used by printf to redirect the output to UART1
 *
 * No need to call it directly use printf instead.
 *
 */
int _write(int file, char *ptr, int len) {
  for (int i = 0; i < len; i++) {
    if (ptr[i] == '\n') {
      HAL_UART_Transmit(&huart2, (uint8_t *)"\r", 1, HAL_MAX_DELAY);
    }
    HAL_UART_Transmit(&huart2, (uint8_t *)&ptr[i], 1, HAL_MAX_DELAY);
  }
  return len;
}

void vprint(const char *fmt, va_list argp) {
  char string[200];
  if (0 < vsprintf(string, fmt, argp)) // build string
  {
    HAL_UART_Transmit(&huart2, (uint8_t *)string, strlen(string),
                      0xffffff); // send message via UART
  }
}

void debugger(const char *fmt, ...) // custom printf() function
{
  va_list argp;
  va_start(argp, fmt);
  vprint(fmt, argp);
  va_end(argp);
}
