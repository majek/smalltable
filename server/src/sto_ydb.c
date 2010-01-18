#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st.h"

#include <ydb.h>

struct ydb_data{
	YDB ydb;
};
typedef struct ydb_data YDB_DATA;


static int get(void *storage_data, char *dst, int dst_sz, char *key, int key_sz) {
	YDB_DATA *ydbd = (YDB_DATA *)storage_data;
	return ydb_get(ydbd->ydb, key, key_sz, dst, dst_sz);
}

static int del(void *storage_data, char *key, int key_sz) {
	YDB_DATA *ydbd = (YDB_DATA *)storage_data;
	return ydb_del(ydbd->ydb, key, key_sz);
}


static int set(void *storage_data, char *value, int value_sz, char *key, int key_sz) {
	YDB_DATA *ydbd = (YDB_DATA *)storage_data;
	return ydb_add(ydbd->ydb, key, key_sz, value, value_sz);
}


static void sync(void *storage_data) {
	YDB_DATA *ydbd = (YDB_DATA *)storage_data;
	ydb_sync(ydbd->ydb);
}

static void prefetch(void *storage_data, char **keys, int *key_szs, int items_counter) {
	YDB_DATA *ydbd = (YDB_DATA *)storage_data;
	unsigned short *sz = (unsigned short*)key_szs;
	int i;
	for(i=0; i<items_counter; i++)
		sz[i] = key_szs[i];
	ydb_prefetch(ydbd->ydb, keys, sz, items_counter);
}


ST_STORAGE_API *storage_ydb_create(int argc, char **argv) {
	if(argc < 1)
		fatal("Please specify database file for YDB");
	if(argc > 1)
		fatal("Only one directory parameter allowed.");
	char *db_file = argv[0];
	
	YDB_DATA *ydbd = (YDB_DATA *)calloc(1, sizeof(YDB_DATA));

	ydbd->ydb = ydb_open(db_file, 4, 512*1024*1024, YDB_CREAT);
	if(ydbd->ydb == NULL) {
		fprintf(stderr, "ydb_create()=NULL\n");
		abort();
	}
	
	ST_STORAGE_API *api = (ST_STORAGE_API *)calloc(1, sizeof(ST_STORAGE_API));
	api->get = &get;
	api->set = &set;
	api->del = &del;
	api->sync = &sync;
	api->prefetch = &prefetch;
	api->storage_data = ydbd;
	return(api);
}

void storage_ydb_destroy(ST_STORAGE_API *api) {
	YDB_DATA *ydbd = (YDB_DATA *)api->storage_data;
	
	ydb_close(ydbd->ydb);
	
	free(ydbd);
	free(api);
}


