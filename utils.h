/**
 * @file utils.h
 * @brief Utility functions for debugging and logging operations.
 * @author pedro
 * @date 2024
 * 
 * This header file provides utility functions commonly used throughout
 * the application, including debugging and logging capabilities.
 */

/**
 * @brief Logs a debug message to the output.
 * 
 * Outputs the provided message for debugging purposes. This function is typically
 * used during development to trace program execution and diagnose issues.
 * 
 * @param mensagem The debug message to be logged. Must be a null-terminated string.
 * 
 * @return void
 * 
 * @note This function should only be used for debugging purposes and may be
 *       disabled in production builds.
 * 
 * @example
 *   log_debug("Application started");
 *   log_debug("Variable x = 42");
 */
#ifndef UTILS_H
#define UTILS_H

#include "common.h"

void log_debug(const char *mensagem);

#endif
