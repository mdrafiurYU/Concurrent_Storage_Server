/**
 * @file
 * @brief This file defines the the structures for user authentication, user params, tables and nodes. Also defines constants and error codes for the server side.
 *
 * The functions here should be implemented in server.c.
 */

#ifndef	SERVER_H
#define SERVER_H

#include "simclist.h"

// Error codes.
#define ERR_INVALID_PARAM 1		///< A parameter is not valid.
#define ERR_NOT_AUTHENTICATED 3		///< Client not authenticated.
#define ERR_AUTHENTICATION_FAILED 4	///< Client authentication failed.
#define ERR_TABLE_NOT_FOUND 5		///< The table does not exist.
#define ERR_KEY_NOT_FOUND 6		///< The key does not exist.
#define ERR_UNKNOWN 7			///< Any other error.
#define ERR_TRANSACTION_ABORT 8		///< Transaction abort error.

/* DANIEL'S CONSTANT*/
#define MAX_LINE_LENGTH 500

// LIMITS
#define MAX_LISTENQUEUELEN 20	///< The maximum number of queued connections.

// Storage server constants.
#define MAX_NUM_OF_TABLES 100		///< Max tables supported by the server.
#define MAX_RECORDS_PER_TABLE 1000 ///< Max records per table.
#define MAX_TABLE_LENGTH 20	///< Max characters of a table name.
#define MAX_KEY_LEN 20		///< Max characters of a key name.
#define MAX_CONNECTIONS 10	///< Max simultaneous client connections.

// Extended storage server constants.
#define MAX_COLUMNS_PER_TABLE 10 ///< Max columns per table.
#define MAX_COLNAME_LEN 20	///< Max characters of a column name.
#define MAX_STRTYPE_SIZE 40	///< Max SIZE of string types.
#define MAX_VALUE_LEN 800	///< Max characters of a value.

#define MAX_PREDICATES 100	///< Max number of predicates.

/**
* @brief Any lines in the config file that start with this character
* are treated as comments.
*/
static const char CONFIG_COMMENT_CHAR = '#';

/**
* @brief A struct to store config parameters.
*/
typedef struct {

	/// If = 1, then it has been set once already
	int hostSet;
	/// If = 1, then it has been set once already
	int portSet;
	/// If = 1, then it has been set once already
	int userSet;
	/// If = 1, then it has been set once already
	int passSet;
	int concurrencySet;
	int tableSet;

	/// The hostname of the server.
	char server_host[MAX_HOST_LEN];

	/// The listening port of the server.
	int server_port;

	/// The storage server's username
	char username[MAX_USERNAME_LEN];

	/// The storage server's encrypted password
	char password[MAX_ENC_PASSWORD_LEN];

	int tableNum;

	/// The directory where tables are stored.
	//	char data_directory[MAX_PATH_LEN];
} config_params;

/**
* @brief A struct to store user authentication.
*/
typedef struct {
	// If user is authenticated
	int authenticated;

	// User provided username
	char username[MAX_USERNAME_LEN];
	// User provided password
	char password[MAX_ENC_PASSWORD_LEN];

	//Socket file descriptor
	int socket;
} user_info;

/**
* @brief A struct to store the list node.
*/
typedef struct {
	// Key of the node
	char key[MAX_KEY_LEN];
	// Value of the columns of the node
	char value[MAX_COLUMNS_PER_TABLE][MAX_STRTYPE_SIZE];
	// metadata
	int metadata;
} census;    /* custom data type to store in list */

/**
* @brief A struct to store the predicate.
*/
typedef struct {
	// Type of predicate
	// -> '0' - String, '1' - Integer
	int type;
	// Value of predicate
	char value[MAX_STRTYPE_SIZE];
	// How the predicate needs to be compared
	// -> '-1' - lesser, '0' - equal, '+1' - greater
	int cmp;
	// Column of predicate
	int colNum;
} predicate;    /* predicates for query */

/**
* @brief A struct to store a table.
*/
typedef struct {
	// Name of the table
	char name[MAX_TABLE_LENGTH];
	// List inside the table
	list_t list;
	int numColumns;
	
	char columnName[MAX_COLUMNS_PER_TABLE][MAX_COLNAME_LEN];
//-1 for integer, <size> for char[size]
	int columnType[MAX_COLUMNS_PER_TABLE];
} table;

//Returns -1 if unsuccessful, returns 1 if successful
//Assumes that column does not exist
int addColumn(table* tab, char* colName, int colType);


/**
* @brief Read and load configuration parameters.
*
* @param config_file The name of the configuration file.
* @param params The structure where config parameters are loaded.
* @return Return 0 on success, -1 otherwise.
*/
//int read_config(const char *config_file);


/**
* @brief Determines if line is an empty string (only whitespace)
*
* @param line The string to check
* @return 1 if empty string.  0 if not empty string.
*/
//int isEmptyString(char *line);

/**
* @brief Modifies str such that there is no trailing whitespace
*
* @param str The string to modify
*/
//void deleteTrailingWhitespace(char * str);
int configConcurrency();
table* getTable(char* tableName, int topTableNumber);
void addTable(char* tableName, int indexToPutAt);
int comparator(const void *a, const void *b);
int seeker(const void *el, const void *key);
int insertRecord(list_t *lp, census *rp, int colNum, int columnType[MAX_COLUMNS_PER_TABLE]);
census* findRecord(list_t *lp, char *keyname);
void displayAllRecords(list_t *lp, int colNum);
void sortAllRecords(list_t *lp, int order);
void list_set_init(list_t *lp);
void ifauthenticate(char *commandstring, int sock, user_info *user);
void ifdataget(char *commandstring, int sock, user_info *user);
void ifdataset(char *commandstring, int sock, user_info *user);
int handle_command(int sock, char *cmd, user_info *user);


int configTable();
int configPassword();
int configUsername();
int configPort();
int isCommentLine(char *line);
int serverConfigParser(const char* config_file);
int processLine(char* line);
int isEmptyString(char *line);
int isCommentLine(char *line);
void deleteTrailingWhitespace(char * str);
int isColSizeValid(char * str);
int processColumnField(char* tableName, char* columnField);
int columnValidConfig(char* table, char* name, char* type, int size);
int addColumnConfig(char* table, char* name, char* type, int size);
int tableValidConfig(char* table);
int addTableConfig(char* table);
int configHost();
#endif


