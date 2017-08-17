/**
 * @file
 * @brief This file implements various utility functions that are
 * can be used by the storage server and client library. 
 */

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "utils.h"
#include <pthread.h>

/* Mutex to guard print statements */ 
pthread_mutex_t  printMutex    = PTHREAD_MUTEX_INITIALIZER; 

/* Mutex to guard condition -- used in getting/returning from thread pool*/ 
pthread_mutex_t conditionMutex = PTHREAD_MUTEX_INITIALIZER;
/* Condition variable -- used to wait for avaiable threads, and signal when available */ 
pthread_cond_t  conditionCond  = PTHREAD_COND_INITIALIZER;


/**
 * @brief Logs to file/stdout based on the LOGGING constant.
 */
int sendall(const int sock, const char *buf, const size_t len)
{
	size_t tosend = len;
	while (tosend > 0) {
		ssize_t bytes = send(sock, buf, tosend, 0);
		if (bytes <= 0) 
			break; // send() was not successful, so stop.
		tosend -= (size_t) bytes;
		buf += bytes;
	};

	return tosend == 0 ? 0 : -1;
}

/**
 * @brief In order to avoid reading more than a line from the stream,
 * this function only reads one byte at a time.  This is very
 * inefficient, and you are free to optimize it or implement your
 * own function.
 */
int recvline(const int sock, char *buf, const size_t buflen)
{
	int status = 0; // Return status.
	size_t bufleft = buflen;

	while (bufleft > 1) {
		// Read one byte from scoket.
		ssize_t bytes = recv(sock, buf, 1, 0);
		if (bytes <= 0) {
			// recv() was not successful, so stop.
			status = -1;
			break;
		} else if (*buf == '\n') {
			// Found end of line, so stop.
			*buf = 0; // Replace end of line with a null terminator.
			status = 0;
			break;
		} else {
			// Keep going.
			bufleft -= 1;
			buf += 1;
		}
	}
	*buf = 0; // add null terminator in case it's not already there.

	return status;
}

/**
 * @brief Logs to file/stdout based on the LOGGING constant.
 */
void logger(int log, FILE *file, char *message)
{
	if(log == 1)			//Logging to screen
	{
		printf("%s\n",message);
	}
	
	else if(log == 2)		//Logging to file
	{
		pthread_mutex_lock( &conditionMutex ); 
		
		fprintf(file,"%s",message);
		fflush(file);

		pthread_mutex_unlock( &conditionMutex ); 	
	}
}

/**
 * @brief Generates an encrypted password by using the UNIX tool called crypt.
 */
char *generate_encrypted_password(const char *passwd, const char *salt)
{
	if(salt != NULL)
		return crypt(passwd, salt);
	else
		return crypt(passwd, DEFAULT_CRYPT_SALT);
}

