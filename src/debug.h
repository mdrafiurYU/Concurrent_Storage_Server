/**
 * @file
 * @brief This file assists in logging at the client side.
 * Stores file pointer for access in client.c and storage.c
 * The LOGGING constant is defined here
 * Includes functions for creating, opening and closing the log file
 *
 *  Created on: 2014-01-19
 *      Author: kumarrus
 */



#ifndef DEBUG_H
#define DEBUG_H




#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#define LOGGING 0

FILE *logFile;

void createLog();

void closeLog();

#endif /* DEBUG_H_ */
