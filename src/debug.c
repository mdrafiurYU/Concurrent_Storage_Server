/**
 * @file
 * @brief This file implements creating and closing  log file on the client side.
 *
 * Set logging = 0 to disable logging
 * Set logging = 1 to enable stdout
 * Set logging = 2 to output to file
 *
 * Created on: 2014-01-19
 *     Author: kumarrus
 */


#include "debug.h"

void createLog()
{
	if(LOGGING == 2)
	{
		time_t timestamp;
		struct tm *tm;

		// Init the tm structure to access current system time
		time(&timestamp);
		tm = localtime (&timestamp);

		// Concat the time variables to create the filename of the log file
		char filename[32];
		snprintf(filename, sizeof filename, "Client-%04d-%02d-%02d-%02d-%02d-%02d.log",
					tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

		// Create the file and open it in write mode
		logFile = fopen(filename, "w");
	}
}

void closeLog()
{
	if (LOGGING == 2)
		fclose(logFile);
}
