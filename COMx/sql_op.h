#ifndef _SQL_OP_H
#define _SQL_OP_H
typedef int (*callback)(void*,int,char**,char**);
typedef void (*select_callback)(void);
typedef void (*update_callback)(void *para);
void sql_select_where_equal(char *key_name, char *value);
void sql_select_where_like(char *key_name, char *value);
void sql_add(char *expression);
int sql_select(char* db_name, char* table_name, select_callback select_record, callback sql_callback, void* para);
int sql_update(char* db_name, char* table_name, select_callback select_record, update_callback update_record, void* para);
int sql_insert(char* db_name, char* table_name, update_callback update_record, void* para);
int sql_delete_tbl(char* db_name, char* table_name);
#endif
