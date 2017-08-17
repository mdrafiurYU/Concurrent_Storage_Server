/**
 * @file
 * @brief This file contains the implementation of the storage server
 * interface as specified in storage.h.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include "storage.h"
#include "utils.h"


/**
 * @brief Function checks if a string only contain alpha numeric characters or not.
 * @param str A string type input.
 * @return Returns a boolian value of 0 if string contains any character that isn't alphanumeric and 1 otherwise.
 */
bool check_alphanum (const char * str)
{
	int i=0;

  	while (isalnum(str[i]))
		i++;

	if(i == strlen(str))
		return true;

	return false;

}



/**
 * @brief Function checks for valid table values, that is if a string only contain alpha numeric characters or spaces.
 * @param str A string type input.
 * @return Returns a boolian value of 0 if string contains any character that isn't alphanumeric or spaces and 1 otherwise.
 */
bool check_valid_value (const char * str)
{
	int i=0;

  	while (isalnum(str[i]) || str[i] == ' ')
		i++;

	if(i == strlen(str))
		return true;

	return false;

}



/**
 * @brief Implemented a connection establishing function according to team design needs.
 */
void* storage_connect(const char *hostname, const int port)
{
	if (hostname == NULL || (hostname && hostname[0] == '\0')) { //Errno for invalid parameter
		errno = ERR_INVALID_PARAM;
		return NULL;
	}
	// Create a socket.
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return NULL;

	// Get info about the server.
	struct addrinfo serveraddr, *res;
	memset(&serveraddr, 0, sizeof serveraddr);
	serveraddr.ai_family = AF_UNSPEC;
	serveraddr.ai_socktype = SOCK_STREAM;
	char portstr[MAX_PORT_LEN];
	snprintf(portstr, sizeof portstr, "%d", port);
	int status = getaddrinfo(hostname, portstr, &serveraddr, &res);
	if (status != 0){
		errno = ERR_CONNECTION_FAIL;
		return NULL;
	}

	// Connect to the server.
	status = connect(sock, res->ai_addr, res->ai_addrlen);
	if (status != 0){
		errno = ERR_CONNECTION_FAIL;
		return NULL;
	}

	return (void*) sock;
}


/**
 * @brief Implemented an authentication function according to team design needs.
 */
int storage_auth(const char *username, const char *passwd, void *conn)
{
	if(conn == NULL || username == NULL || passwd == NULL || (username && username[0] == '\0') || (passwd && passwd[0] == '\0')) { //Error for invalid parameter
		errno = ERR_INVALID_PARAM;

	} else {
		// Connection is really just a socket file descriptor.
		int sock = (int)conn;

		int status, err;

		// Send some data.
		char buf[MAX_CMD_LEN];
		memset(buf, 0, sizeof buf);
		char *encrypted_passwd = generate_encrypted_password(passwd, NULL);
		snprintf(buf, sizeof buf, "AUTH,%s,%s\n", username, encrypted_passwd);
		if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0) {
			sscanf( buf, "%d,%d", &status, &err);
			//printf("%s\n", buf);
			errno = err;
			if(errno != 0)
				return -1;
			return 0;
		}
	}
	return -1;
}

/**
 * @brief Implemented a get key function according to team design needs.
 */
int storage_get(const char *table, const char *key, struct storage_record *record, void *conn)
{
	 if (conn == NULL || key == NULL || table == NULL || record == NULL || (key && key[0] == '\0') || (table && table[0] == '\0') || !check_alphanum (table) || !check_alphanum (key)) { //Errno for invalid parameter
		errno = ERR_INVALID_PARAM;

	} else {
		// Connection is really just a socket file descriptor.
		int sock = (int)conn;
		int metadata;
		int status, err;
		char value[50];

		// Send some data.
		char buf[MAX_CMD_LEN];
		memset(buf, 0, sizeof buf);
		snprintf(buf, sizeof buf, "GET,%s,%s\n", table, key);
		if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0) {
			sscanf( buf, "%d%*[ ,]%d%*[ ,]%d%*[ ,]%[^\n]", &status, &err, &metadata, value );
			//printf("%s\n", value);
			errno = err;
			record->metadata[0] = metadata;
			if(errno != 0)
				return -1;
			record->metadata[0] = metadata;
			snprintf(record->value, sizeof record->value, "%s", value);
			return 0;
		}
	}
	return -1;
}





/**
 * @brief Implemented a query key function according to team design needs.
 */
int storage_query(const char *table, const char *predicates, char **keys, const int max_keys, void *conn)
{
	 if (conn == NULL || keys == NULL || (predicates && predicates[0] == '\0') || (table && table[0] == '\0') || !check_alphanum (table) || max_keys < 0) { //Errno for invalid parameter
		errno = ERR_INVALID_PARAM;

	} else {
		// Connection is really just a socket file descriptor.
		int sock = (int)conn;

		int status, err;
		int number_of_keys;
		//char *keys[max_keys];

		// Send some data.
		char buf[MAX_CMD_LEN];
		memset(buf, 0, sizeof buf);
		snprintf(buf, sizeof buf, "QUERY,%s,%d,%s\n", table, max_keys, predicates);

		if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0) {


			int i = 0;
    			int param_num = 0;
    			char * pch;
  			pch = strtok (buf,",");

  			while (pch != NULL)
  			{
   				if (param_num==0)
				{
        				status = atoi(pch);
				}

    				else if (param_num==1)
				{
        				err = atoi(pch);
				}

				else if (param_num==2)
				{
        				number_of_keys = atoi(pch);
				}


    				else if(param_num>2)
				{
    					strcpy(keys[i], pch);
    					i++;
    				}

				pch = strtok (NULL, ",");
    				param_num++;

			}

			errno = err;

			if(errno != 0)
				return -1;
			//if(errno == 0)
				//snprintf(record->value, sizeof record->value, "%s", value);	//No error so do whatever with the keys array.

			return number_of_keys;
		}
	}
	return -1;
}





/**
 * @brief Implemented a set value function according to team design needs.
 */
int storage_set(const char *table, const char *key, struct storage_record *record, void *conn)
{
	 if (conn == NULL || key == NULL || table == NULL || (key && key[0] == '\0') || (table && table[0] == '\0') || !check_alphanum (table) || !check_alphanum (key)) { //Errno for invalid parameter
		errno = ERR_INVALID_PARAM;

	} else {
		// Connection is really just a socket file descriptor.
		int sock = (int)conn;

		int status, err;

		// Send some data.
		char buf[MAX_CMD_LEN];
		memset(buf, 0, sizeof buf);
		if(record == NULL)
			snprintf(buf, sizeof buf, "SET,%s,%s,0,NULL NULL\n", table, key);
		else
			snprintf(buf, sizeof buf, "SET,%s,%s,%d,%s\n", table, key, record->metadata[0], record->value);
		if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0) {
			sscanf( buf, "%d,%d", &status, &err);
			//printf("%s\n", buf);
			errno = err;
			if(errno != 0)
				return -1;
			return 0;
		}
	}
	return -1;
}


/**
 * @brief Implemented a disconnection function according to team design needs.
 */
int storage_disconnect(void *conn)
{
	if (conn == NULL) { //Errno for invalid parameter
		errno = ERR_INVALID_PARAM;
		return -1;
	}
	// Cleanup
	int sock = (int)conn;
	
	char buf[MAX_CMD_LEN] = "DISCONN";

	if(sendall(sock, buf, strlen(buf)) == 0){
		close(sock);
	}

	return 0;
}

