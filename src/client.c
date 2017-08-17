/**
 * @file
 * @brief This file implements a "very" simple sample client.
 * 
 * The client connects to the server, running at SERVERHOST:SERVERPORT
 * and performs a number of storage_* operations. If there are errors,
 * the client exists.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "storage.h"

#include <sys/types.h>
#include <time.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "debug.h"
#include <stdlib.h>

/*
#define SERVERHOST "localhost"
#define SERVERPORT 1111
#define SERVERUSERNAME "admin"
#define SERVERPASSWORD "dog4sale"
#define TABLE "marks"
#define KEY "ece297"
*/

int GET_META = 0;
int abortCount = 0;

void *conn;

enum choice
{
	CONNECT = 1,
	AUTH = 2,
	GET = 3,
	SET = 4,
	QUERY = 5,
	DISCONNECT = 6,
	EXIT = 7
};

void print_menu()
{
	printf("> ---------------------\n");
	printf("> 1) Connect\n");
	printf("> 2) Authenticate\n");
	printf("> 3) Get\n");
	printf("> 4) Set\n");
	printf("> 5) Query\n");
	printf("> 6) Disconnect\n");
	printf("> 7) Exit\n");
	printf("> ---------------------\n");
	printf("> Enter your choice: ");
}

int server_connect()
{
	// Input from user
	char *SERVERHOST = readline("> HOSTNAME: ");
	int SERVERPORT;
	PORT_INPUT:
	{
		char *userInput = readline("> PORT: ");
		int is_valid = sscanf(userInput, "%d", &SERVERPORT);
		free(userInput); // Free memory alloc'd by readline

		if(is_valid != 1)
		{
			printf("Please enter a valid input.\n");
			goto PORT_INPUT;
		}
	}
	// Connect to server
	printf("> Connecting to %s:%d ...\n", SERVERHOST, SERVERPORT);
	conn = storage_connect(SERVERHOST, SERVERPORT);
	if(!conn) {
		printf("Cannot connect to server @ %s:%d. Error code: %d.\n",
			   SERVERHOST, SERVERPORT, errno);
		free(SERVERHOST);	// Free memory alloc'd by readline
		return -1;
	}
	printf("storage_connect: successful.\n");
	free(SERVERHOST);	// Free memory alloc'd by readline
	return 0;
}

int server_auth(int *status)
{
	// Input from user
	char *SERVERUSERNAME = readline("> USERNAME: ");
	char *SERVERPASSWORD = readline("> PASSWORD: ");

	// Authenticate the client.
	*status = storage_auth(SERVERUSERNAME, SERVERPASSWORD, conn);
	if(*status != 0) {
		printf("storage_auth failed with username '%s' and password '%s'. " \
			   "Error code: %d.\n", SERVERUSERNAME, SERVERPASSWORD, errno);
		storage_disconnect(conn);
		free(SERVERUSERNAME);	// Free memory alloc'd by readline
		free(SERVERPASSWORD);	// Free memory alloc'd by readline
		return -1;
	}
	if(errno != 0) {
		printf("storage_auth:failed, error_code:%d\n", errno);
		errno = 0;

	} else {
		printf("storage_auth: successful.\n");

	}	
	free(SERVERUSERNAME);	// Free memory alloc'd by readline
	free(SERVERPASSWORD);	// Free memory alloc'd by readline
	return 0;
}

int get_record(int *status)
{
	// Input from user
	char *TABLE = readline("> Enter TABLE: ");
	char *KEY = readline("> Enter KEY: ");

	// Issue storage_get
	struct storage_record r;
	*status = storage_get(TABLE, KEY, &r, conn);
	if(*status != 0) {
		printf("storage_get failed. Error code: %d.\n", errno);
		//storage_disconnect(conn);
		//free(TABLE);	// Free memory alloc'd by readline
		//free(KEY);	// Free memory alloc'd by readline
		return -1;
	}
	if(errno != 0) {
		printf("storage_get:failed, error_code:%d\n", errno);
		errno = 0;

	} else {
		printf("storage_get: the value returned for key '%s' is '%s'.\n",
		 KEY, r.value);
		GET_META = r.metadata[0];

	}
	//free(TABLE);	// Free memory alloc'd by readline
	//free(KEY);	// Free memory alloc'd by readline
	return 0;
}

int set_record(int *status)
{
	int METADATA_VAL = 0;
	// Input from user
	char *TABLE = readline("> Enter TABLE: ");
	char *KEY = readline("> Enter KEY: ");
	char *VALUE = readline("> Enter VALUE: ");
	char *userInput = readline("> TRANSACTION?: ");
	int is_valid = sscanf(userInput, "%d", &METADATA_VAL);
	if(METADATA_VAL == 0) {
		METADATA_VAL = GET_META;
	}
	// Issue storage_set
	struct storage_record r;
	strncpy(r.value, VALUE, sizeof r.value);
	r.metadata[0] =  METADATA_VAL;
	*status = storage_set(TABLE, KEY, &r, conn);
	if(*status != 0) {
		printf("storage_set failed. Error code: %d.\n", errno);
		//storage_disconnect(conn);
		//free(TABLE);	// Free memory alloc'd by readline
		//free(KEY);	// Free memory alloc'd by readline
		//free(VALUE);	// Free memory alloc'd by readline
		return -1;
	}
	if(errno != 0) {
		printf("storage_set:failed, error_code:%d\n", errno);
		errno = 0;

	} else {
		printf("storage_set: successful.\n");

	}
	//free(TABLE);	// Free memory alloc'd by readline
	//free(KEY);	// Free memory alloc'd by readline
	//free(VALUE);	// Free memory alloc'd by readline
	return 0;
}

int server_disconnect(int *status)
{
	// Disconnect from server
	*status = storage_disconnect(conn);
	if(*status != 0) {
		printf("storage_disconnect failed. Error code: %d.\n", errno);
		return -1;
	}
	printf("storage_disconnect: successful.\n");
	return 0;
}

int server_query(int *status)
{
	char* tableName = readline("> Enter TABLE: ");
	char* predicates = readline("> Enter PREDICATES: ");
	char** keys = NULL;
	char* m_keys = readline("> Enter MAX KEYS: ");
	int max_keys = atoi(m_keys);
	
	keys = malloc(max_keys * sizeof(char*));
    	int i=0;
    	for (i = 0; i < max_keys; i++) {
		keys[i] = malloc((MAX_KEY_LEN) * sizeof(char));
    	}

	// Disconnect from server
	*status = storage_query(tableName, predicates, keys, max_keys, conn);
	if(*status == -1) {
		printf("storage_query failed. Error code: %d.\n", errno);
		return -1;
	}
	printf("storage_query: successful. Found: %d records\n", *status);
	return 0;
}

/**
 * @brief Start a client to interact with the storage server.
 *
 * If connect is successful, the client performs a storage_set/get() on
 * TABLE and KEY and outputs the results on stdout. Finally, it exists
 * after disconnecting from the server.
 */
int main(int argc, char *argv[])
{
	int userChoice, status;

	int counter = 0;
	printf("ABC");


	// If logging is enabled create the log file
	//createLog();

	do {
		print_menu();

		CHOICE_INPUT:
		{
			


			char *userInput = readline("");
			int is_valid = sscanf(userInput, "%d", &userChoice);
			free(userInput); // Free memory alloc'd by readline

			if(is_valid != 1)
			{
				printf("Please enter a valid input.\n");
				printf("> Enter your choice: ");
				goto CHOICE_INPUT;
			}
		}

		switch(userChoice)
		{
			case CONNECT:
				if(server_connect() == -1)
					return -1;
				break;

			case AUTH:
				if(server_auth(&status) == -1)
					return status;
				break;

			case GET:
				printf("Before GET : ERRNO = %d\n", errno);
				if(get_record(&status) == -1) {
					if(errno != 5 && errno != 6 && errno != 8)
						return status;
					if (errno == 8)
						abortCount++;
				}
				printf("AFTER GET : ERRNO = %d\n", errno);
				break;

			case SET:
				printf("Before SET : ERRNO = %d\n", errno);
				if(set_record(&status) == -1) {
					if(errno != 5 && errno != 6 && errno != 8 && errno != 9)
						return status;
					if (errno == 8)
						abortCount++;
				}
				printf("AFTER SET : ERRNO = %d\n", errno);
				break;

			case DISCONNECT:
				if(server_disconnect(&status) == -1)
					return status;
				break;
			case QUERY:
				if(server_query(&status) == -1)
					return status;
				break;

			case EXIT: break;

			default : printf("Please enter a valid choice.\n");
		}
	}while(userChoice != 7);

	// If logging is enabled, close the file that has been opened
	printf("%d", abortCount);
	// Exit
	return 0;
}
