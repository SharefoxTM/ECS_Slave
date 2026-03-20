/*
 * log.h
 *
 *  Created on: Nov 7, 2025
 *      Author: wda
 *
 * This simple logging library supports different verbosity levels.
 * Log levels can be configured per file where you include the header file.
 * A message must meet or exceed its log level to be printed.
 * look at log.c for an example
 * The default log level is INFO and print in color
 * define any of the log levels BEFORE including the log.h header
 * 	LOG_LEVEL_NONE, LOG_LEVEL_ERROR, LOG_LEVEL_WARN, LOG_LEVEL_INFO,
 * LOG_LEVEL_DEBUG, LOG_LEVEL_VERBOSE define LOG_NO_COLOR to disable the colors
 */

#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>

#ifdef LOG_LEVEL_NONE
#define LOG_LEVEL 6
#endif
#if LOG_LEVEL_ERROR
#define LOG_LEVEL 5
#endif
#ifdef LOG_LEVEL_WARN
#define LOG_LEVEL 4
#endif
#ifdef LOG_LEVEL_INFO
#define LOG_LEVEL 3
#endif
#ifdef LOG_LEVEL_DEBUG
#define LOG_LEVEL 2
#endif
#ifdef LOG_LEVEL_VERBOSE
#define LOG_LEVEL 1
#endif

#ifdef LOG_NO_COLOR
#define TERM_COLOR 0
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif
#ifndef TERM_COLOR
#define TERM_COLOR 1
#endif

#if TERM_COLOR == 1
#define TERM_COLOR_BLACK "\033[0;30m"   // Black
#define TERM_COLOR_RED "\033[0;31m"     // Red
#define TERM_COLOR_GREEN "\033[0;32m"   // Green
#define TERM_COLOR_YELLOW "\033[0;33m"  // Yellow
#define TERM_COLOR_BLUE "\033[0;34m"    // Blue
#define TERM_COLOR_MAGENTA "\033[0;35m" // Magenta (ook vaak paars genoemd)
#define TERM_COLOR_CYAN "\033[0;36m"    // Cyan
#define TERM_COLOR_WHITE "\033[0;37m"   // White
#define TERM_COLOR_RESET "\033[0m"      // Reset naar standaardkleur en -stijl
#define TERM_COLOR_BRIGHT_BLACK "\033[0;90m"   // Bright Black
#define TERM_COLOR_BRIGHT_RED "\033[0;91m"     // Bright Red
#define TERM_COLOR_BRIGHT_GREEN "\033[0;92m"   // Bright Green
#define TERM_COLOR_BRIGHT_YELLOW "\033[0;93m"  // Bright Yellow
#define TERM_COLOR_BRIGHT_BLUE "\033[0;94m"    // Bright Blue
#define TERM_COLOR_BRIGHT_MAGENTA "\033[0;95m" // Bright Magenta
#define TERM_COLOR_BRIGHT_CYAN "\033[0;96m"    // Bright Cyan
#define TERM_COLOR_BRIGHT_WHITE "\033[0;97m"   // Bright White
#else
#define TERM_COLOR_BLACK
#define TERM_COLOR_RED
#define TERM_COLOR_GREEN
#define TERM_COLOR_YELLOW
#define TERM_COLOR_BLUE
#define TERM_COLOR_MAGENTA
#define TERM_COLOR_CYAN
#define TERM_COLOR_WHITE
#define TERM_COLOR_RESET
#define TERM_COLOR_BRIGHT_BLACK
#define TERM_COLOR_BRIGHT_RED
#define TERM_COLOR_BRIGHT_GREEN
#define TERM_COLOR_BRIGHT_YELLOW
#define TERM_COLOR_BRIGHT_BLUE
#define TERM_COLOR_BRIGHT_MAGENTA
#define TERM_COLOR_BRIGHT_CYAN
#define TERM_COLOR_BRIGHT_WHITE
#endif

// LOG_LEVEL_NONE -> no messages

#if LOG_LEVEL <= 1
#define LOG_VERBOSE(fmt, ...)                                                  \
  debugger("[" TERM_COLOR_GREEN "Verbose" TERM_COLOR_RESET                     \
           "] %8lums [%s:%d]: " fmt "\n",                                      \
           logGetTimestamp(), logGetFileName(__FILE__), __LINE__,              \
           ##__VA_ARGS__)
#else
#define LOG_VERBOSE(fmt, ...)
#endif

#if LOG_LEVEL <= 2
#define LOG_DEBUG(fmt, ...)                                                    \
  debugger("[" TERM_COLOR_MAGENTA "Debug" TERM_COLOR_RESET                     \
           "]   %8lums [%s:%d]: " fmt "\n",                                    \
           logGetTimestamp(), logGetFileName(__FILE__), __LINE__,              \
           ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if LOG_LEVEL <= 3
#define LOG_INFO(fmt, ...)                                                     \
  debugger("[" TERM_COLOR_WHITE "Info" TERM_COLOR_RESET                        \
           "]    %8lums [%s:%d]: " fmt "\n",                                   \
           logGetTimestamp(), logGetFileName(__FILE__), __LINE__,              \
           ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL <= 4
#define LOG_WARN(fmt, ...)                                                     \
  debugger("[" TERM_COLOR_YELLOW "Warn" TERM_COLOR_RESET                       \
           "]    %8lums [%s:%d]: " fmt "\n",                                   \
           logGetTimestamp(), logGetFileName(__FILE__), __LINE__,              \
           ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL <= 5
#define LOG_ERROR(fmt, ...)                                                    \
  debugger("[" TERM_COLOR_RED "Error" TERM_COLOR_RESET                         \
           "]   %8lums [%s:%d]: " fmt "\n",                                    \
           logGetTimestamp(), logGetFileName(__FILE__), __LINE__,              \
           ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

long logGetTimestamp(void);
void logInit(void);
char *logGetFileName(char *file);
void debugger(const char *fmt, ...);
#endif /* LOG_H */
