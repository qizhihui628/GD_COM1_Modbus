#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "sql_op.h"
#include "pthread.h"
static pthread_mutex_t mymutex=PTHREAD_MUTEX_INITIALIZER;
char base_exp[200];

void sql_add(char *expression)
{
	strcat(base_exp, expression);
}

void sql_select_where_equal(char *key_name, char *value)
{
	printf("base_exp: %s\r\n", base_exp);
	sql_add(key_name);
	sql_add("=\'");
	sql_add(value);
	sql_add("\'");
	printf("base_expf: %s\r\n", base_exp);

}

void sql_select_where_less(char *key_name, char *value)
{
	printf("base_exp: %s\r\n", base_exp);
	sql_add(key_name);
	sql_add("<=\'");
	sql_add(value);
	sql_add("\'");
	printf("base_expf: %s\r\n", base_exp);

}

void sql_clear_where(void)
{
	strcpy(base_exp, "");
	printf("base_expf: %s\r\n", base_exp);
}

int sql_select(char* db_name, char* table_name, select_callback select_record,
        callback sql_callback, void* para)
{
	sqlite3 * db = 0;
	int result = 0;
	char ** errmsg = NULL;
	pthread_mutex_lock(&mymutex);
	result = sqlite3_open(db_name, &db);

	if (result != SQLITE_OK)
	{
		printf("open db: %s failed error: %d \r\n", db_name, result);
		result = -1;
		goto error;
	}
	sql_clear_where();
	strcat(base_exp, "select * from ");
	strcat(base_exp, table_name);
	strcat(base_exp, " where ");
	select_record();
	result = sqlite3_exec(db, base_exp, sql_callback, para, errmsg);
	if (result != SQLITE_OK)
	{
		printf("open sql table: light1 failed\r\n");
		result = -1;
		goto error;
	}
error:
	sqlite3_close(db);
	pthread_mutex_unlock(&mymutex);

	return result;
}

int sql_update(char* db_name, char* table_name, select_callback select_record,
        update_callback update_record, void* para)
{
	sqlite3 * db = 0;
	int result = 0;
	char ** errmsg = NULL;
	pthread_mutex_lock(&mymutex);
	result = sqlite3_open(db_name, &db);

	if (result != SQLITE_OK)
	{
		printf("open db: %s failed error: %d \r\n", db_name, result);
		result = -1;
		goto error;
	}
	sql_clear_where();
	strcat(base_exp, "update ");
	strcat(base_exp, table_name);
	strcat(base_exp, " set ");
	update_record((int *) para);
	strcat(base_exp, " where ");
	select_record();
	result = sqlite3_exec(db, base_exp, NULL, NULL, errmsg);
	if (result != SQLITE_OK)
	{
		printf("open sql table: %s failed\r\n", table_name);
		result = -1;
		goto error;
	}

error:
	sqlite3_close(db);
	pthread_mutex_unlock(&mymutex);
	return result;
}

int sql_insert(char* db_name, char* table_name, update_callback update_record, void* para)
{
	sqlite3 * db = 0;
	int result = 0;
	char ** errmsg = NULL;
	pthread_mutex_lock(&mymutex);
	result = sqlite3_open(db_name, &db);

	if (result != SQLITE_OK)
	{
		printf("open db: %s failed error: %d \r\n", db_name, result);
		result = -1;
		goto error;
	}

	sql_clear_where();
	strcat(base_exp, "insert into ");
	strcat(base_exp, table_name);
	strcat(base_exp, " values ");
	update_record((int *) para);
	printf("insert base_exp: %s\r\n", base_exp);
	result = sqlite3_exec(db, base_exp, NULL, NULL, errmsg);
	if (result != SQLITE_OK)
	{
		printf("open sql table: %s failed\r\n", table_name);
		result = -1;
		goto error;
	}
error:
	sqlite3_close(db);
	pthread_mutex_unlock(&mymutex);
	return result;
}

int sql_delete_tbl(char* db_name, char* table_name)
{
	sqlite3 * db = 0;
	int result = 0;
	char ** errmsg = NULL;
	pthread_mutex_lock(&mymutex);
	result = sqlite3_open(db_name, &db);

	if (result != SQLITE_OK)
	{
		printf("open db: %s failed error: %d \r\n", db_name, result);
		result = -1;
		goto error;
	}

	sql_clear_where();
	strcat(base_exp, "delete from ");
	strcat(base_exp, table_name);
	result = sqlite3_exec(db, base_exp, NULL, NULL, errmsg);
	if (result != SQLITE_OK)
	{
		printf("open sql table: %s failed\r\n", table_name);
		result = -1;
		goto error;
	}
error:
	sqlite3_close(db);
	pthread_mutex_unlock(&mymutex);
	return result;
}
