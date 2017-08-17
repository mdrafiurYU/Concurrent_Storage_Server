/**
 * @file
 * @brief This file implements the storage server.
 *
 * The storage server should be named "server" and should take a single
 * command line argument that refers to the configuration file.
 *
 * The storage server should be able to communicate with the client
 * library functions declared in storage.h and implemented in storage.c.
 *
 * Used an open source data structure library called SimCList. 
 * 
 * SimCList library. See http://mij.oltrelinux.com/devel/simclist
 * Copyright (c) 2007,2008 Mij <mij@bitchx.it>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <stdbool.h>
#include "utils.h"
#include "server.h"
#include <time.h>

// Threading
#include <pthread.h>
#include <semaphore.h>

#define LOGGING 1

int strClearBoundWS(char* str);

FILE* serverLog;
table tables[MAX_NUM_OF_TABLES];
config_params params;
//user_info user;
int concurrency;


/** 
 * @brief Structure to indentify a particular thread 
 */
typedef struct {
	pthread_t pth;	// this is our thread identifier
	bool active;
} thread;
thread threadpool[MAX_CONNECTIONS];

// Condition variable -- used to wait for avaiable threads, and signal when available 
pthread_mutex_t  getLock  = PTHREAD_MUTEX_INITIALIZER;

/** 
 * @brief Creates a File for logging 
 * @return Returns a FILE pointer type to the server log
 */
FILE* createLog()
{
	time_t timestamp;
	struct tm *tm;

	// Init the tm structure to access current system time
	time(&timestamp);
	tm = localtime (&timestamp);

	// Concat the time variables to create the filename of the log file
	char filename[32] = {0};
	snprintf(filename, sizeof filename, "Server-%04d-%02d-%02d-%02d-%02d-%02d.log",
				tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	// Create the file and open it in write mode
	FILE *file;
	file=fopen(filename, "w");

	return file;
}




/** 
 * @brief Function returns the size of each node in the list. It is used for malloc purposes.
 * @param el Points a node in the list.
 * @return Returns size of structure census.
 */
size_t meter(const void *el) {
    return sizeof(census);
}




/** 
 * @brief Function to find if the node->key is same as given key.
 * @param el Points a node in the list.
 * @param key Contains the key given by the user
 * @return Returns true when the key of the node matches the key given.
 */
int seeker(const void *el, const void *key) {
    /* let's assume el and key being always != NULL */
    const census *cont = (census *)el;
    const char *key_city = (char *)key;

    if (!strcmp(cont->key, key_city))
        return 1;
    return 0;
}





/**
 * @brief Function to insert a node in the list
 * @param lp A pointer to the list
 * @param rp A pointer to the new node
 * @param colNum Number of columns in the table
 * @param colType An array containing the column types of all the columns(i.e string,int)
 * @return Returns void
 */
int insertRecord(list_t *lp, census *rp, int colNum, int columnType[MAX_COLUMNS_PER_TABLE]) {
    census *tuple = (census *)list_seek(lp, rp->key);
    int j=0;
    for(j=0; j< colNum; j++) {
	if(columnType[j] > 0) {
		int indexTrim = columnType[j];
		if(indexTrim < MAX_STRTYPE_SIZE)
			rp->value[j][indexTrim-1] = 0;
		else
			rp->value[j][MAX_STRTYPE_SIZE-1] = 0;
	}
    }
    if(tuple == NULL) {
		if(rp->metadata != 0)
			return -1;
		rp->metadata = 1;
        list_append(lp, rp);
    }
    else {
		if(rp->metadata != tuple->metadata && rp->metadata != 0)
			return -1;
		int i;
		for(i=0; i < colNum; i++) {
			strcpy(tuple->value[i], rp->value[i]);
		}
		tuple->metadata++;
    }
    return 0;
}




/**
 * @brief Function to find record into the list
 * @param lp A pointer to the list
 * @param keyname A string containing the key
 * @return Returns a pointer to the found tuple. Returns NULL if nothing is found
 */
census* findRecord(list_t *lp, char *keyname) {
    census *tuple = (census *)list_seek(lp, keyname);
    return tuple;
}


/**
 * @brief Function to find record into the list
 * @param lp A pointer to the list
 * @param rp A pointer to the census report
 * @return Returns a pointer to the found tuple. Returns NULL if nothing is found
 */
int deleteRecord(list_t *lp, census *rp) {
    census *tuple = (census *)list_seek(lp, rp->key);
    if(tuple == NULL) {
	return -1;
    } 
    list_delete(lp, tuple);
    return 0; 
}


/**
 * @brief Function to display all records in the list
 * @param lp A pointer to the list
 * @param colNum Column index
 * @return Returns void
 */
void displayAllRecords(list_t *lp, int colNum) {
    /* [display list ...] */
    census *data;
    list_iterator_start(lp);        /* starting an iteration "session" */
    while (list_iterator_hasnext(lp)) { /* tell whether more values available */
        data = (census *)list_iterator_next(lp);
        printf("%s:\n", data->key);
	int i;
	for(i=0; i < colNum; i++) {
		printf("\t::%s\n", data->value[i]);
	} 
    }
    list_iterator_stop(lp);
}




/**
 * @brief Function called for sorting the linked list
 * @param lp A pointer to the list
 * @param order 1 -> ascending , -1 -> descending
 * @return Returns void
 */
void sortAllRecords(list_t *lp, int order) {
    list_sort(lp, order);
}



/**
 * @brief Function to query all stored records
 * @param lp A pointer to the list
 * @param colPreds[MAX_PREDICATES] A string containing all predicates to query for
 * @param numPreds Integer number of provided perdicates
 * @param keys_arr An array of stings containing all keys found by the query function
 * @param max_keys Integer maximum number of keys to be found by the query function provided by the client
 * @return Returns Integer number of keys found with matching predicates 
 */
int queryAllRecords(list_t *lp, predicate colPreds[MAX_PREDICATES], int numPreds, char **keys_arr, int max_keys) {
    int keys_count = 0;
    census *record;
    list_iterator_start(lp);        /* starting an iteration "session" */
    bool passFlag;
    while (list_iterator_hasnext(lp)) { /* tell whether more values available */
        record = (census *)list_iterator_next(lp);
	passFlag = true;
	int i;
	for(i=0; i < numPreds && passFlag; i++) {
		//Query each Predicate here
		int columnNo = colPreds[i].colNum;
		if(colPreds[i].type >= 0) {	/* Predicate is string type */
			if(strcmp(colPreds[i].value, record->value[columnNo]) != 0) {
				passFlag = false;
			}
		} else { /* Predicate is integer type */
			switch(colPreds[i].cmp) {
				case -1: /* lesser than */
					 if(!(strtol(record->value[columnNo], NULL, 10) < 
						strtol(colPreds[i].value, NULL, 10))) {
					 	 passFlag = false;
					 }
					 break;
				case 0: /* equal to */
					 if(!(strtol(record->value[columnNo], NULL, 10) == 
						strtol(colPreds[i].value, NULL, 10))) {
					 	 passFlag = false;
					 }
					 break;
				case 1: /* greater than */
					 if(!(strtol(record->value[columnNo], NULL, 10) > 
						strtol(colPreds[i].value, NULL, 10))) {
					 	 passFlag = false;
					 }
					 break;
			}
		}
	}
	if(passFlag) {
		/* Record satisfies all predicates */
		
		if(keys_count < max_keys) {
			printf("f:%s:\n", record->key);
			strcpy(keys_arr[keys_count], record->key);
		}
		keys_count++;
	}
    }
    list_iterator_stop(lp);
    return keys_count;
}


/**
 * @brief Function to initialize the list. This also sets the custom meter and seeker functions
 * @param lp A pointer to the list
 * @return Returns void
 */
void list_set_init(list_t *lp) {

    list_init(lp);

    /* setting the custom spanning function */
    list_attributes_copy(lp, meter, 1);

    /* set the custom seeker function */
    list_attributes_seeker(lp, seeker);

    /* setting the custom comparator */
    //list_attributes_comparator(lp, comparator);
}





/**
 * @brief Function to find the table in the array of tables
 * @param tableName The name of the table to find
 * @param topTableNumber Number of tables in the array
 * @return Returns a pointer to the table if found. Returns NULL otherwise
 */
table* getTable(char* tableName, int topTableNumber){
	int i;

	for (i=0; i < topTableNumber; i++) {
		if (strcmp(tables[i].name, tableName) == 0)
			return &(tables[i]);
	}
	return NULL;
}



/**
 * @brief Function to add a new table to the array of tables
 * @param tableName The name of the table to insert
 * @param indexToPutAt Index to put the table
 * @return Returns void
 */
void addTable(char* tableName, int indexToPutAt){
	strcpy(tables[indexToPutAt].name, tableName);
	list_set_init(&(tables[indexToPutAt].list));
}



/**
 * @brief Function to find a column in a table
 * @param tab The name of the table to search
 * @param colName Name of the column to be found
 * @param num The index of the column to be found
 * @param type The type of value that the column holds
 * @return Returns Integer -1 if column isn't' found and 0 if the specified column is found
 */
int findColumn(table* tab, char colName[MAX_COLNAME_LEN], int *num, int *type) {
	int i=0;	
	for(i=0; i< tab->numColumns; i++) {
		if(strcmp(tab->columnName[i], colName) == 0) {
			*num = i;
			*type = tab->columnType[i];
			return 0;
		}
	}
	return -1;
}




/**
 * @brief Function to remove spaces preciding a particular string
 * @param str A character pointer pointing to a string
 * @return Returns The character string pointer with all preciding spaces removed
 */
char *trimbeforespace(char *str) {
	char *end;

	// Trim leading space
	while(isspace(*str)) 
		str++;

	if(*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) 
		end--;

	// Write new null terminator
	*(end+1) = 0;

	return str;
}



/**
 * @brief Function to remove spaces following a particular string
 * @param str A character pointer pointing to a string
 * @return Returns The character string pointer with all following spaces removed
 */
char *trimafterspace(char *str) {
	char *start = str;
	while(*str != 0) 
		str++;

	str--;
	// Trim leading space
	while(isspace(*str)) {
		*str = '\0';
		str--;
	}
	//if(*str == 0)  // All spaces?
	str = start;
	return str;
}





/**
 * @brief Function to remove spaces preciding and following a particular string by calling functions trimafterspaces and trimbeforespaces
 * @param str A character pointer pointing to a string
 * @return Returns The character string pointer with al
l preciding and following spaces removed
 */
char* tok_helper (char *str) {
	str = trimbeforespace(str);
	str = trimafterspace(str);
	return str;
}




/**
 * @brief Function breaks down string and compare parameters to authenticate user
 * @param commandstring A string type
 * @param sock An integer type which specifies the socket you want to connect to.
 * @return Returns nothing (void).
 */
void ifauthenticate(char *commandstring, int sock, user_info *user)
{	
	char command[MAX_USERNAME_LEN] = {0};
	char username[MAX_USERNAME_LEN] = {0};
	char password[MAX_ENC_PASSWORD_LEN] = {0};
	char logMessage[MAX_LOG_LEN] = {0};
	int paramnumber = 0;
	
	char buf[MAX_CMD_LEN];
	memset(buf, 0, sizeof buf);
	
	//Unmarshall the TCP message
	char *saveptr;
	char *pch = strtok_r(commandstring, ",", &saveptr);

	while(pch != NULL){
		paramnumber++;

		if(paramnumber == 2) {
			strcpy(username, tok_helper (pch));

		} else if(paramnumber == 3) {
			strcpy(password, tok_helper (pch));

		}
		pch = strtok_r(NULL, ",", &saveptr);
	}
	if(paramnumber != 3) {
		snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
		sendall(sock, buf, strlen(buf));
		return;
    	}
	//sscanf(commandstring, "%s,%s,%s", command, username, password);
	if(strcmp(params.username, username) != 0 || strcmp(params.password, password) != 0) {
		//AUTH failed with the given username and password
		snprintf(logMessage, sizeof logMessage, "Authentication failed %d:%d\n", strcmp(params.username, username), strcmp(params.password, password));
		logger(LOGGING, serverLog, logMessage);
		snprintf(buf, sizeof buf, "0,%d\n", ERR_AUTHENTICATION_FAILED);
		sendall(sock, buf, strlen(buf));
		return;
	}
	// Authentication successfull
	user->authenticated = 1;
	snprintf(logMessage, sizeof logMessage, "Authenticated with username: %s and password: %s, %s\n", username, password, command);
	logger(LOGGING, serverLog, logMessage);
	snprintf(buf, sizeof buf, "1,0\n");
	sendall(sock, buf, strlen(buf));
}




/**
 * @brief Function breaks down string, checks for authentication and finds user defined data from specified table.
 * @param commandstring A string type	
 * @param sock An integer type that specifies the socket system is working on.
 * @return Returns nothing (void).
 */
void ifdataget(char *commandstring, int sock, user_info *user)
{
    char buf[MAX_CMD_LEN];
    memset(buf, 0, sizeof buf);
    char logMessage[MAX_LOG_LEN] = {0};

    if(user->authenticated == 0) {
	// USER not authenticated
	snprintf(buf, sizeof buf, "0,%d,0,0\n", ERR_NOT_AUTHENTICATED);
	sendall(sock, buf, strlen(buf));
	return;
    }
    char* data_key;
    char* data_table;

    int paramnumber = 0;
    char *saveptr;
    char * pch = strtok_r(commandstring, ",", &saveptr);

    while(pch != NULL){
		paramnumber++;

		if(paramnumber == 2) {
			data_table = tok_helper (pch);

		} else if(paramnumber == 3) {
			data_key = tok_helper (pch);
		
		}
		pch = strtok_r(NULL, ",", &saveptr);

    }
    if(paramnumber != 3) {
	snprintf(buf, sizeof buf, "0,%d,0,0\n", ERR_INVALID_PARAM);
	sendall(sock, buf, strlen(buf));
	return;
    }
	// find the table and pointer to its list
	table* t = getTable(data_table, params.tableNum);
	if(t == NULL) {
		//TABLE not found
		snprintf(buf, sizeof buf, "0,%d,0,0\n", ERR_TABLE_NOT_FOUND);
		sendall(sock, buf, strlen(buf));
		return;
	}
	// find the record in the list
	pthread_mutex_lock( &getLock );
	census* tuple = findRecord(&(t->list), data_key);
	pthread_mutex_unlock( &getLock );
	if(tuple == NULL) {
		//RECORD not found
		snprintf(buf, sizeof buf, "0,%d,0,0\n", ERR_KEY_NOT_FOUND);
		sendall(sock, buf, strlen(buf));
		return;
	}
        //snprintf(logMessage, sizeof logMessage, "Got value: %s from table: %s and key: %s\n", tuple->value[0], data_table, data_key);
	//logger(LOGGING, serverLog, logMessage);
	char bufTemp[MAX_CMD_LEN];
	memset(bufTemp, 0, sizeof bufTemp);
    	int i=0;
    	for(i=0; i<t->numColumns; i++) {
		strcat(bufTemp, t->columnName[i]);
		strcat(bufTemp, " ");
		strcat(bufTemp, tuple->value[i]);
		if(i != t->numColumns - 1)
			strcat(bufTemp, ",");
    	}
	//printf(":%s:", bufTemp);
	snprintf(buf, sizeof buf, "1,0,%d,%s\n", tuple->metadata, bufTemp);
	sendall(sock, buf, strlen(buf));
}




/**
 * @brief Function to check if column sequence follows the same order as config file
 * @parameter t A pointer to the table to check columns
 * @parameter columns An array of strings containing names of columns
 * @parameter numCol Number of columns in the table
 * @parameter value An array of strings containing values to be inserted
 */
int check_columnname_error (table *t, char columns[MAX_COLUMNS_PER_TABLE][MAX_COLNAME_LEN], int numCol, char value[MAX_COLUMNS_PER_TABLE][MAX_STRTYPE_SIZE]) {

    //check if i == colNum
    if(numCol != t->numColumns)
	return -1;

    	
    int j = 0;
    while (j < numCol){
    	if(strcmp (columns[j], t->columnName[j]) != 0)
		return -1;
	j++;
    }
    return 0;
}




/**
 * @brief Function checks for valid table values, that is if a string only contain alpha numeric characters or spaces.
 * @param str A string type input.
 * @return Returns a boolian value of 0 if string contains any character that isn't alphanumeric or spaces and 1 otherwise.
 */
bool check_valid_string (const char * str)
{
	int i=0;
  	while (isalnum(str[i]) || str[i] == ' ')
		i++;

	if(i == strlen(str))
		return true;

	return false;

}



/**
 * @brief Function checks for valid signed integer entered as a string
 * @param str A string type input.
 * @return Returns a boolian value of true if string contains a signed integer and false if it has any character but a sign or number 
 */
bool check_valid_integer (const char * str)
{
	int i=0,s=0, d=0;
  	while (i < strlen(str)) {
		if(str[i] == '-' || str[i] == '+')
			s++;
		if(isdigit(str[i]))
			d++;
		i++;
	}

	if(s+d == strlen(str) && s<=1)
		return true;

	return false;

}




/**
 * @brief Function checks for valid column value
 * @param tab A pointer to the table.
 * @param val Set of values to be checked and inserted into the table
 * @return Returns an integer value of 0 if value is valid and -1 otherwise
 */
int checkColumnVal(table *tab, char val[MAX_COLUMNS_PER_TABLE][MAX_STRTYPE_SIZE]) {
	int i=0;	
	for(i=0; i< tab->numColumns; i++) {
		if(tab->columnType[i] >= 0) { //Column is of string type
			//check if the string is valid value
			if(!check_valid_string(val[i]))
				return -1;
		} else {
			//check if integer is valid value
			if(!check_valid_integer(val[i]))
				return -1;
		}
	}
	return 0;
}




/**
 * @brief Function breaks down string, checks for authentication and retrieves user defined data from specified table.
 * @param commandstring A string type	
 * @param sock An integer type that specifies the socket system is working on.
 * @return Returns nothing (void).
 */
void ifdataset(char *commandstring, int sock, user_info *user)
{
    char buf[MAX_CMD_LEN];
    memset(buf, 0, sizeof buf);
	
    if(user->authenticated == 0) {
	// USER not authenticated
	snprintf(buf, sizeof buf, "0,%d\n", ERR_NOT_AUTHENTICATED);
	sendall(sock, buf, strlen(buf));
	return;
    }
    char data_table[MAX_TABLE_LENGTH] = {0};
    char columnName[MAX_COLUMNS_PER_TABLE][MAX_COLNAME_LEN];

    int numColumns = 0;
    census record;
    int paramnumber = 0;
    char tempS[40] = {0};

    char *saveptr;
    char * pch = strtok_r(commandstring, ",", &saveptr);

    while(pch != NULL){
		paramnumber++;

		if(paramnumber == 2) {
			strcpy(data_table, tok_helper (pch));

		} else if(paramnumber == 3) {
			strcpy(record.key, tok_helper (pch));

		} else if(paramnumber == 4) {
			record.metadata = atoi(pch);

		} else if(paramnumber >= 5) {
	
			strcpy(tempS, pch);

			int count_pred_val = 0;
			char *saveptr2;
			char *ptemp = strtok_r(tempS, " ", &saveptr2);
			while(ptemp != NULL) {
				if(count_pred_val >= 1) {
					strcpy(record.value[numColumns], tok_helper (ptemp));
				} else {
					strcpy(columnName[numColumns], tok_helper (ptemp));
				}				
				count_pred_val++;
				if(count_pred_val >= 1)
					ptemp = strtok_r(NULL, "\0", &saveptr2);
				else
					ptemp = strtok_r(NULL, " ", &saveptr2);
			}

			numColumns++;

		}
		pch = strtok_r(NULL, ",", &saveptr);
    }
  
    if(paramnumber < 5) {
	snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
	sendall(sock, buf, strlen(buf));
	return;
    }

	// find the table and pointer to its list
	table *t = getTable(data_table, params.tableNum);
	if(t == NULL) {
		//TABLE not found
		snprintf(buf, sizeof buf, "0,%d\n", ERR_TABLE_NOT_FOUND);
		sendall(sock, buf, strlen(buf));
		return;
	}
	
	if(strcmp(record.value[0],"NULL") == 0) {
		pthread_mutex_lock( &getLock );
		int delStatus = deleteRecord(&(t->list), &record);
		pthread_mutex_unlock( &getLock );
		if(delStatus == -1) {
			//RECORD not found
			snprintf(buf, sizeof buf, "0,%d\n", ERR_KEY_NOT_FOUND);
			sendall(sock, buf, strlen(buf));
			return;
		}
		snprintf(buf, sizeof buf, "1,0\n");
		sendall(sock, buf, strlen(buf));
		return;
	}

	//Check if coloumns are in correct order	
	if(check_columnname_error (t, columnName, numColumns, record.value) == -1 ||
		checkColumnVal(t,record.value) == -1){
		snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
		sendall(sock, buf, strlen(buf));
		return;
	}
	pthread_mutex_lock( &getLock );
	// insert the record in the list
	if(insertRecord(&(t->list), &record, t->numColumns, t->columnType) == -1)
	{
		pthread_mutex_unlock( &getLock );
		snprintf(buf, sizeof buf, "0,%d\n", ERR_TRANSACTION_ABORT);
		sendall(sock, buf, strlen(buf));
		return;
	}
	pthread_mutex_unlock( &getLock );
	snprintf(buf, sizeof buf, "1,0\n");
	sendall(sock, buf, strlen(buf));
}


/**
 * @brief Function breaks down string, checks for authentication and queries for user defined data from specified table.
 * @param commandstring A string type	
 * @param sock An integer type that specifies the socket system is working on.
 * @return Returns nothing (void).
 */
void ifdataquery(char *commandstring, int sock, user_info *user)
{
    char buf[MAX_CMD_LEN];
    memset(buf, 0, sizeof buf);
	
    if(user->authenticated == 0) {
	// USER not authenticated
	snprintf(buf, sizeof buf, "0,%d\n", ERR_NOT_AUTHENTICATED);
	sendall(sock, buf, strlen(buf));
    	return;
    }

    char data_table[MAX_TABLE_LENGTH] = {0};
	
    predicate inputPreds[MAX_PREDICATES];
    int maxKeys;
    int paramnumber = 0;
    int numPreds = 0;
    char tempS[40] = {0}, tempS2[40] = {0};

    table *t;
    //printf("%s\n", commandstring);
    char *saveptr;
    char * pch = strtok_r(commandstring, ",", &saveptr);

    while(pch != NULL){
		paramnumber++;

		if(paramnumber == 2) {
			strcpy(data_table, tok_helper (pch));

		} else if(paramnumber == 3) {
			maxKeys = atoi(tok_helper (pch));

		} else if(paramnumber >= 4) {
			strcpy(tempS, tok_helper (pch));
			strcpy(tempS2, tok_helper (pch));
			//char *tempS2p = tempS2;
			// pred_val[0] -> name of the column
			// pred_val[1] -> value of predicate
			char pred_val[2][40];

			// sign value of predicate -> '>,<,='
			char pred_sign_val;
			
			int count_pred_val = 0;
			char *saveptr2;
			char *ptemp = strtok_r(tempS, "><=", &saveptr2);
			while(ptemp != NULL && count_pred_val < 2) {
				strcpy(pred_val[count_pred_val], tok_helper (ptemp));
				count_pred_val++;
				ptemp = strtok_r(NULL, "><=", &saveptr2);
			}
			if((count_pred_val == 2 && ptemp != NULL) || (ptemp == NULL && count_pred_val != 2)) {
				//printf("INVALID_PARAM:strtok\n");
				snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
				sendall(sock, buf, strlen(buf));
				return;
			}
				
			char tempS3[40] = {0}, tempS4[40] = {0};				
			int scount = sscanf(tempS2, "%[a-zA-Z0-9] %c %s", tempS3, &pred_sign_val, tempS4);
			if(scount != 3) {
				//printf("INVALID_PARAM:sscanf\n");
				snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
				sendall(sock, buf, strlen(buf));
				return;
			}
			//printf(":%c:\n", pred_sign_val);
			inputPreds[numPreds].cmp = -2;		

			if(pred_sign_val == '<') {
				inputPreds[numPreds].cmp = -1;
			} else if(pred_sign_val == '=') {
				inputPreds[numPreds].cmp = 0;
			} else if(pred_sign_val == '>') {
				inputPreds[numPreds].cmp = 1;
			} else {
				//printf("INVALID_PARAM:signval\n");
				snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
				sendall(sock, buf, strlen(buf));
				return;
			}

			strcpy(inputPreds[numPreds].value, pred_val[1]);
			
			// find the table and pointer to its list
			t = getTable(data_table, params.tableNum);			
			if(t == NULL) {
				//TABLE not found
				snprintf(buf, sizeof buf, "0,%d\n", ERR_TABLE_NOT_FOUND);
				sendall(sock, buf, strlen(buf));
				return;
			}

			int columnNumber, columnType;
			if(findColumn(t,pred_val[0], &columnNumber, &columnType) == -1) {
				//printf("INVALID_PARAM:column\n");
				snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
				sendall(sock, buf, strlen(buf));
				return;
			}

			inputPreds[numPreds].colNum = columnNumber;
			inputPreds[numPreds].type = columnType;
			
			numPreds++;
		}
		pch = strtok_r(NULL, ",", &saveptr);

    }

    if(paramnumber < 4) {
	snprintf(buf, sizeof buf, "0,%d\n", ERR_INVALID_PARAM);
	sendall(sock, buf, strlen(buf));
	return;
    }

    char **result_arr;

    result_arr = malloc(maxKeys * sizeof(char*));
    int i=0;
    for (i = 0; i < maxKeys; i++) {
	result_arr[i] = malloc((MAX_KEY_LEN) * sizeof(char));
    }
    pthread_mutex_lock( &getLock );
    int numKeysFound = queryAllRecords(&(t->list), inputPreds, numPreds, result_arr, maxKeys);
    pthread_mutex_unlock( &getLock );
    //printf(":%d:", numKeysFound);
    char bufTemp[MAX_CMD_LEN];
    memset(bufTemp, 0, sizeof bufTemp);
    int limit = (maxKeys < numKeysFound)?maxKeys:numKeysFound;

    for(i=0; i<limit; i++) {
	strcat(bufTemp, result_arr[i]);
	if(i != limit -1)
		strcat(bufTemp, ",");
    }
    snprintf(buf, sizeof buf, "1,0,%d,%s\n", numKeysFound, bufTemp);
    sendall(sock, buf, strlen(buf));
}




/**
 * @brief Function checks the first parameter of the string to check for user command and calls user requested commands.
 * @param cmd A string type	
 * @param sock An integer type that specifies the socket system is working on.
 * @return Returns nothing (void).
 */
int handle_command(int sock, char *cmd, user_info *user)
{
	char logMessage[MAX_LOG_LEN] = {0};
	snprintf(logMessage, sizeof logMessage, "Processing command '%s'\n", cmd);
	logger(LOGGING, serverLog, logMessage);

	// For now, just send back the command to the client.
	//---------
	char inputstring[MAX_CMD_LEN] = {0};
	strcpy(inputstring, cmd);

	char * pch;
	char * mt_saveptr;
	pch = strtok_r (cmd,", ",&mt_saveptr);

	if (pch != NULL)
	{
		if(!strcmp(pch, "AUTH")) {   //Checking each command to call corresponding
			ifauthenticate(inputstring, sock, user); //String broken down into parts inside each function
			
		} else if(!strcmp(pch, "GET")) {
			ifdataget(inputstring, sock, user);
			
		} else if(!strcmp(pch, "SET")) {
			ifdataset(inputstring, sock, user);
			
		} else if(!strcmp(pch, "QUERY")) {
			ifdataquery(inputstring, sock, user);
			
		} else if(!strcmp(pch, "DISCONN")) {
			user->authenticated = 0;
			
		} else {
			snprintf(logMessage, sizeof logMessage, "Error: Invalid command\n");
			logger(LOGGING, serverLog, logMessage);
			return -1;
		}
		
	} else {
		//printf("Error: Wrong format or Null command\n");
		snprintf(logMessage, sizeof logMessage, "Error: Wrong format or Null command\n");
		logger(LOGGING, serverLog, logMessage);
		return -1;
	}
	//---------
	//sendall(sock, buf, strlen(buf));
	//sendall(sock, "\n", 1);
	
	return 0;
}


/**
 * @brief Function to handle concurrent client commands.
 * @param arguments A void pointer type.	
 * @return Returns a void pointer.
 */
void* clientHandler(void* arguments) {
	// Get commands from client.
	//printf("In the handler");
	user_info user;
	int threadNum;
	sscanf((char*)arguments, "%d %d", &(user.socket), &threadNum);
	user.authenticated = 0;
	int wait_for_commands = 1;
	do {
		// Read a line from the client.
		char cmd[MAX_CMD_LEN] = {0};
		int status = recvline(user.socket, cmd, MAX_CMD_LEN);
		if (status != 0) {
			// Either an error occurred or the client closed the connection.
			wait_for_commands = 0;
		} else {
			// Handle the command from the client.
			int status = handle_command(user.socket, cmd, &user);
			if (status != 0)
				wait_for_commands = 0; // Oops.  An error occured.
		}
	} while (wait_for_commands);

	// Close the connection with the client.
	close(user.socket);
	user.authenticated = 0;
	threadpool[threadNum].active = false;
	//printf("Connection Closed by a client!");
	
	return NULL;
}


/**
 * @brief Start the storage server.
 *
 * This is the main entry point for the storage server.  It reads the
 * configuration file, starts listening on a port, and proccesses
 * commands from clients.
 */
int main(int argc, char *argv[])
{
	bool load_workload = false;

	if(LOGGING == 2)
		serverLog = createLog();
	else
		serverLog = NULL;

	// Process command line arguments.
	// This program expects exactly one argument: the config file name.
	assert(argc > 0);
	if(argc == 3 && !strcmp(argv[2], "workload")) {
		printf("NOTE: workload enabled.\n");
		load_workload = true;	
	}
	else if (argc != 2) {
		printf("Usage %s <config_file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	char *config_file = argv[1];
	
	params.hostSet = 0;
	params.portSet = 0;
	params.passSet = 0;
	params.userSet = 0;
	params.tableNum = 0;
	params.concurrencySet = 0;
	params.tableSet = 0;

	// Read the config file.
	int status = serverConfigParser(config_file);
	if (status != 1) {
		printf("Error processing config file.\n");
		exit(EXIT_FAILURE);
	}
	char logMessage[MAX_LOG_LEN] = {0};
	snprintf(logMessage, sizeof logMessage, "Server on %s:%d\n", params.server_host, params.server_port);
	logger(LOGGING, serverLog, logMessage);

	if(load_workload) {
		FILE *fin;            /* declare the file pointer */
		fin = fopen ("../data/census/workload.txt", "r");  /* open the file for reading */
		char line[80] = {0};
		census record;
		char t[MAX_TABLE_LENGTH] = {0};
		printf("Enter table to be populated: ");
		scanf("%s",t);
		// find the table and pointer to its list
		table *wtable = getTable(t, params.tableNum);
		if(wtable == NULL) {
			//TABLE not found
			printf("Missing table <%s>\n", t);
			exit(EXIT_FAILURE);
		}

		/* acquire census data and insert in list ... */
		while(fgets(line, 80, fin) != NULL) {
			/* get a line, up to 80 chars from fr.  done if NULL */
			sscanf (line, "%s %[^\n]", record.key, record.value[0]);
			insertRecord(&(wtable->list), &record, wtable->numColumns, wtable->columnType);
		}
		fclose(fin);  /* close the file prior to exiting the routine */

	}

	// Create a socket.
	int listensock = socket(PF_INET, SOCK_STREAM, 0);
	if (listensock < 0) {
		printf("Error creating socket.\n");
		exit(EXIT_FAILURE);
	}

	// Allow listening port to be reused if defunct.
	int yes = 1;
	status = setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
	if (status != 0) {
		printf("Error configuring socket.\n");
		exit(EXIT_FAILURE);
	}

	// Bind it to the listening port.
	struct sockaddr_in listenaddr;
	memset(&listenaddr, 0, sizeof listenaddr);
	listenaddr.sin_family = AF_INET;
	listenaddr.sin_port = htons(params.server_port);
	inet_pton(AF_INET, params.server_host, &(listenaddr.sin_addr)); // bind to local IP address
	status = bind(listensock, (struct sockaddr*) &listenaddr, sizeof listenaddr);
	if (status != 0) {
		printf("Error binding socket.\n");
		exit(EXIT_FAILURE);
	}

	// Listen for connections.
	status = listen(listensock, MAX_LISTENQUEUELEN);
	if (status != 0) {
		printf("Error listening on socket.\n");
		exit(EXIT_FAILURE);
	}

	int j=0;
	for(j=0; j<MAX_CONNECTIONS; j++) {
		threadpool[j].active = false;
	}
	// Listen loop.
	int wait_for_connections = 1;
	while (wait_for_connections) {
		// Wait for a connection.
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof clientaddr;
		int clientsock = accept(listensock, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (clientsock < 0) {
			printf("Error accepting a connection.\n");
			exit(EXIT_FAILURE);
		}

		snprintf(logMessage, sizeof logMessage, "Got a connection from %s:%d.\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
		logger(LOGGING, serverLog, logMessage);
		if(concurrency == 1) {
			bool findthread = false;
			while(findthread != true) {
				int i=0;
				for(i=0; i<MAX_CONNECTIONS; i++) {
					if(threadpool[i].active == false) {
						char threadArg[MAX_STRTYPE_SIZE];
						snprintf(threadArg, sizeof threadArg, "%d %d", clientsock, i);
						pthread_create(&(threadpool[i].pth),NULL,clientHandler,threadArg);
						threadpool[i].active = true;
						findthread = true;
						break;
					}
				}
			}
		} else {
			threadpool[0].active = true;
			char threadArg[MAX_STRTYPE_SIZE];
			snprintf(threadArg, sizeof threadArg, "%d 0", clientsock);
			pthread_create(&(threadpool[0].pth),NULL,clientHandler,threadArg);
			while(threadpool[0].active == true);
		}
//		snprintf(logMessage, sizeof logMessage, "Closed connection from %s:%d.\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
//		logger(LOGGING, serverLog, logMessage);
	}

	// Stop listening for connections.
	close(listensock);

	if(LOGGING == 2)
		fclose(serverLog);

	return EXIT_SUCCESS;
}




/**
 * @brief Starting point for config parsing, so must start here.
 * @param config_file A pointer to the config file
 * @return Returns -1 if file is invalid, 1 if file is valid
 */
int serverConfigParser(const char* config_file){
	int error_occurred = 0;

	// Open file for reading.
	FILE *config = fopen(config_file, "r");
	if (config == NULL)
		return 1;

	//Will exit loop when read the whole file
	while ((error_occurred == 0)){
		char line[MAX_LINE_LENGTH] = {0};
		if (feof(config))
			break;
		fgets(line, MAX_LINE_LENGTH, config);
		printf("GOT LINE : %s\n", line);
		printf("FEOF SAYS: %d\n", feof(config));
		deleteTrailingWhitespace(line);
		error_occurred = processLine(line);
		printf("Line Failed? : %d\n", error_occurred);
		printf("FINISH PROCESS LINE\n\n");
	}

	//If an error occurred, then config file is invalid
	if (error_occurred == 1)
		return -1;

	//If all the parameters weren't set, then config file is invalid
	if (params.hostSet == 0 || params.portSet == 0 || params.passSet == 0 || params.userSet == 0 || params.concurrencySet == 0 || params.tableSet == 0){
	printf("%d\n", params.hostSet);
	printf("%d\n", params.portSet);
	printf("%d\n", params.passSet);
	printf("%d\n", params.userSet);
	printf("%d\n", params.concurrencySet);
	printf("%d\n", params.tableSet);
		return -1;
	}

	return 1;
}




/**
 * @brief Processes each line in configuration file.
 * @param line A pointer to a string, which is a line in the configuration file.
 * @return Returns 1 if line is invalid, 0 if line is valid.
 */
int processLine(char* line){
	if (isEmptyString(line) == 1)
		return 0;

	if (isCommentLine(line) == 1)
		return 0;

	//Get parameter name
	char* parameter = strtok(line, ", \r\t");
	if (strcmp(parameter, "server_host") == 0)
		return configHost();
	else if (strcmp(parameter, "server_port") == 0)
		return configPort();
	else if (strcmp(parameter, "username") == 0)
		return configUsername();
	else if (strcmp(parameter, "password") == 0)
		return configPassword();
	else if (strcmp(parameter, "table") == 0)
		return configTable();
	else if (strcmp(parameter, "concurrency") == 0)
		return configConcurrency();
	else if (isEmptyString(line))
		return 0;
	else
		return 1;

	return 0;

}


/**
 * @brief Function to check concurrency parameter value
 * @return Returns 0 if successful and 1 if failure.
 */
int configConcurrency(){
	char* concurrencyValue = strtok(NULL, ", \r\t");
	if (concurrencyValue == NULL)
		return 1;
	if (check_valid_integer(concurrencyValue) == false)
	return 1;
	if (params.concurrencySet == 1)
		return 1;
	//Determine if portname field already defined
	if (params.concurrencySet == 0)
		params.concurrencySet = 1;
	
	int val = atoi(concurrencyValue);
	if (val == 0 || val == 1)
		concurrency = val;
	else
		return 1;
	return 0;
}



/**
 * @brief Responsible for setting up hostname parameter in server.
 * @return Returns 1 if hostname was already set in a previous config line, or hostname is invalid
 */
int configHost(){
	char* hostname = strtok(NULL, ", \r\t");
	char* additionalArgs = strtok(NULL, ", \r\t");
	if ((hostname == NULL) || (additionalArgs != NULL))
		return 1;

	//Determine if portname field already defined
	if (params.hostSet == 1)
		return 1;
	strcpy(params.server_host, hostname);
	params.hostSet = 1;

	return 0;
}



/**
 * @brief Responsible for setting up port parameter in server.
 * @return Returns 1 if port was already set in a previous config line, or port is invalid
 */
int configPort(){
	char* portname = strtok(NULL, ", \r\t");
	char* additionalArgs = strtok(NULL, ", \r\t");

	if ((portname == NULL) || (additionalArgs != NULL))
		return 1;

	//Determine if portname field already defined
	if (params.portSet == 1)
		return 1;
	params.server_port = atoi(portname);
	params.portSet = 1;

	return 0;
}



/**
 * @brief Responsible for setting up username parameter in server.
 * @return Returns 1 if usernmae was already set in a previous config line, or usernmae is invalid
 */
int configUsername(){
	char* username = strtok(NULL, ", \r\t");
	char* additionalArgs = strtok(NULL, ", \r\t");

	if ((username == NULL) || (additionalArgs != NULL))
		return 1;

	//Determine if username field already defined
	if (params.userSet == 1)
		return 1;
	strcpy(params.username, username);
	params.userSet = 1;

	return 0;
}



/**
 * @brief Responsible for setting up password parameter in server.
 * @return Returns 1 if password was already set in a previous config line, or password is invalid
 */
int configPassword(){
	char* password = strtok(NULL, ", \r\t");
	char* additionalArgs = strtok(NULL, ", \r\t");

	if ((password == NULL) || (additionalArgs != NULL))
		return 1;

	//Determine if password field already defined
	if (params.passSet == 1)
		return 1;
	strcpy(params.password, password);
	params.passSet = 1;
	return 0;
}



/**
 * @brief Responsible for setting up table parameter in server.
 * @return Returns 1 if table and its columns are valid, 0 if duplicate/invalid table or column(s), or if no columns defined
 */
int configTable(){
	//Tablename should be anything after "table" but before any column names
	char* tableName = strtok(NULL, " \t");
	//If no tableName is given (ERROR)
	if (tableName == NULL)
		return 1;

	//See if table exists in the list (If tableValid == 0, ERROR)
	if (tableValidConfig(tableName) == 0)
		return 1;

	addTableConfig(tableName);

	//Process each column field.  It should be delimited by commas
	//If it is not delimited  by commas, then we get a huge string, which would be caught by processColumnField
	char* columnField = strtok(NULL, ",");

	//If no columns are defined, then ERROR
	if (columnField == NULL)
		return 1;

	//Delete any whitespace between the column declarations
	columnField = columnField + strClearBoundWS(columnField);
	int a = processColumnField(tableName, columnField);

	//Process rest of column fields
	while ((columnField != NULL) && (a == 0)){
		//Each columnfield should be delimited by ',' ONLY
		columnField = strtok(NULL, ",");
		//Clear any whitespace as a result of not ignoring them
		columnField = columnField + strClearBoundWS(columnField);
		//If we didn't get anything, then we've read the entire line
		if (columnField == NULL) break;
		//Process the line if it exists at all
		a = processColumnField(tableName, columnField);
	}
	//columnField = strtok(NULL, ", \t");
	//if (columnField != NULL)
	//		return 1;

	//If we got a = 1, then processing field was unsuccessful, ERROR
	if (a == 1)
		return 1;

	//No errors to report from this table line
	return 0;
} //end function configTable



/**
 * @brief Processes the columns in a given table in the config file
 * @param tableName The name of the table
 * @param columnField The column field (ex. "name:int" or "city:char[20]")
 * @return Returns 1 if all columns defined are valid. 0 if duplicate columns or invalid column type
 */
int processColumnField(char* tableName, char* columnField){
	//Some strings that may contain values
	char a1[200] = "";
	char a2[200] = "";
	char a3[200] = "";
	char a4[200] = "";
	int invalidColField = 0;
	//printf("%s\n", columnField);
	//fflush(stdout);
	//Form of <name>:<type>[size]
	sscanf(columnField, "%[^:]:%[^[][%[^]]]%s", a1, a2, a3, a4);
	printf("---%s-%s-%s-%s---", a1, a2, a3, a4); //<<Test to check parsing
	//If type is not 'char', then we need to reparse
	if (strcmp(a2, "char") != 0) {
		//Form of <name>:<type><random stuff (if there is any>
		//If correct, a1 will be name, a2 will be "int"
		sscanf(columnField, "%[^:]:%s%s", a1, a2, a3);
		if (strcmp(a3, "") != 0)
			return 1;
	}
	printf("---%s-%s-%s-%s---", a1, a2, a3, a4); //<<Test to check parsing
	printf("Got : %s - %s - %s - %s\n", a1, a2, a3, a4);
	//Check if there are any invalid characters in field

	invalidColField = strcmp(a4, "");
	if (invalidColField != 0)
		return 1;
	//If it is a char, we're expecting a valid number in a3
	if (strcmp(a2, "char") == 0){
		//printf("Name : %s, Size : %s\n", a1, a3);
		if (isColSizeValid(a3) == 1){
			printf("CONFIG : Column size invalid\n");
			return 1;
		}
		//If the column isn't valid, reuturn 1, else add column to table
		if (columnValidConfig(tableName, a1, a2, atoi(a3)) == 0){
			printf("CONFIG : Column value invalid\n");
			return 1;
		}
		printf("Add column\n");
		if (addColumnConfig(tableName, a1, a2, atoi(a3)) == 1)
			return 1;
	}
	//If it is int, we don't expect anything for the size
	else if (strcmp(a2, "int") == 0){
		printf("Process int 1\n");
		//If the column isn't valid, reuturn 1, else add column to table
		if (columnValidConfig(tableName, a1, a2, 0) == 0){
			return 1;
		}
		printf("Process int 2\n");
		if (addColumnConfig(tableName, a1, a2, 0) == 1)
			return 1;
		printf("Process int 3\n");
	}
	else{ //a2 is neither int nor char (invalid data type)
		return 1;
	}
	return 0;

} //end function processColumnField



/**
 * @brief Determines if a string is empty (no characters at all)
 * @param line A String type
 * @return Returns 1 if string is empty, 0 if string is not empty
 */
int isEmptyString(char *line){
	int len = strlen(line);
	int i;

	//Go through each character to see if it is a space
	//If there is one that isn't a space, then return 0 (string not empty)
	for (i = 0; i < len; i++){
		if (!isspace(line[i]))
			return 0;
	}
	return 1;
}



/**
 * @brief Determines if a config line is a comment line (starts with '#')
 * @param line A String type
 * @return Returns 1 if line is a comment, 0 if line is not a comment
 */
int isCommentLine(char *line){
	if (line[0] == '#')
		return 1;
	return 0;
}



/**
 * @brief Deletes the last character if it's a new line
 * @param str A String type
 */
void deleteTrailingWhitespace(char * str){
	if (str[strlen(str) - 1] == '\n')
		str[strlen(str) - 1] = '\0';
}



/**
 * @brief Determines if a column size for the config file is valid (no negative, must be int)
 * @param str A String type
 * @return Returns 1 if it is valid, 0 if it is not valid.
 */
int isColSizeValid(char * str){
	//Check if string is empty - then invalid
	if (strcmp(str, "") == 0)
		return 1;

	//Check if number is negative - then invalid
	if (*str == '-')
		return 1;

	//Check if string is an int - then invalid
	while (*str){
		if (!isdigit(*str))
			return 1;
		str++;
	}

	return 0;
}




/**
 * @brief Determines if a column already exists or is invalid, or max columns reached
 * @param tableName Name of the table
 * @param name Name of the column
 * @param type Type of the column
 * @param size Size of the column if type is char
 * @return Returns 0 if it is valid, -1 if it is not valid.
 */
int columnValidConfig(char* tableName, char* name, char* type, int size){

	if (name == NULL || tableName == NULL)
		return 1;
	
	int i = 0;
	//All column names shall be alphanumeric
	for (i = 0; i < strlen(name); i++){
		if (isalnum(name[i]) == 0)
			return 1;
	}

	table* tab = getTable(tableName, params.tableNum);
	//table not found
	if (tab == NULL)
		return 1;

	//findColumn = 0 -> cannot find column
	int dummy = 0;
	int a =  findColumn(tab, name, &dummy, &dummy);
	if (a != 1){
		return 1;
	}

	return 0;
}




/**
 * @brief Adds a column to the table specified
 * @param tabName Name of the table
 * @param name Name of the column
 * @param type Type of the column
 * @param size Size of the column if type is char
 * @return Returns 0 if successful, -1 if not successful
 */
int addColumnConfig(char* tabName, char* name, char* type, int size){

	table* tab = getTable(tabName, params.tableNum);
	int colType;	
	
	//if int, then type is -1, if char, type is size
	if (strcmp(type, "int") == 0)
		colType = -1;
	else
		colType = size;

	int a = addColumn(tab, name, colType);

	if (a == -1){
		return 1;
	}

	return 0;
}




/**
 * @brief Sees if adding a table would be valid
 * @param tableName Name of the table
 * @return Returns 1 if it is valid, 0 if it is not valid.
 */
int tableValidConfig(char* tableName){
	table* tab = getTable(tableName, params.tableNum);

	//If tab == NULL (no table of name exists right now)
	if (tab == NULL)
		return 1;
	//If tab != NULL (i.e. it exists)
	return 0;
}



/**
 * @brief Function to add a column to a table assuming that the column does not exist
 * @param tab A pointer to the table
 * @param colName The name of the column
 * @param colType An integer indicating the type of column to be created
 * @return Returns Integer 1 if unsuccessful, returns 0 if successful
 */
int addColumn(table* tab, char* colName, int colType){	
	
	//Check if we can't put anymore data in (max columns reached)
	if (tab->numColumns == MAX_COLUMNS_PER_TABLE){
		return -1;
	}
	if(colType == 0) {
		return -1;
	}

	//Copies column name
	strcpy(tab->columnName[tab->numColumns], colName);
	tab->columnType[tab->numColumns] = colType;
	tab->numColumns++;

	return 0;
} 



/**
 * @brief Adds a table
 * @param tablename Name of the table
 * @return Returns 1 if successful, 0 if not successful
 */
int addTableConfig(char* tableName){
	if (params.tableSet == 0)
		params.tableSet = 1;
	addTable(tableName, params.tableNum);
	params.tableNum++;
	return 0;
}




/**
 * @brief Clears all whitespace at end and before of string
 * @param str Name of the table
 * @return Returns integer offset pointer must take to remove whitespace before string
 */
int strClearBoundWS(char* str){
	int forwardShift = 0;

	if (str == NULL)
		return 0;

	while ((str[strlen(str) - 1] == ' ') || (str[strlen(str) - 1] == '\n') || (str[strlen(str) - 1] == '\t'))
		str[strlen(str) - 1] = '\0';


	while ((*str == ' ') || (*str == '\t')){
		str++;
		forwardShift++;
	}
	return forwardShift;
}
