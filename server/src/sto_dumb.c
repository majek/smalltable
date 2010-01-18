#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st.h"

int dumb_get(void *storage_data, char *dst, int size, char *key, int key_sz){
	return(-1);
}

int dumb_del(void *storage_data, char *key, int key_sz) {
	return(0);
}

int dumb_set(void *storage_data, char *value, int value_sz, char *key, int key_sz) {
	return(value_sz);
}

void dumb_sync(void *storage_data) {
}


ST_STORAGE_API *storage_dumb_create(int argc, char **argv) {
	if(argc)
		fatal("This engine doesn't take any parameters.");
	ST_STORAGE_API *api = (ST_STORAGE_API *)calloc(1, sizeof(ST_STORAGE_API));
	api->get = &dumb_get;
	api->set = &dumb_set;
	api->del = &dumb_del;
	api->sync = &dumb_sync;
	api->storage_data = NULL;
	return(api);
}

void storage_dumb_destroy(ST_STORAGE_API *api) {
	free(api);
}
