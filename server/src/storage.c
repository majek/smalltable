#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "st.h"

/*
always: 
 FIRST  - read key, copy it, whatever
 SECOND - write value (Response)
 
 assumption -> more free space after value, see __reserved_for_cas
 
 KEY should not be in the same memory as VALUE!
*/

int storage_get(ST_STORAGE_API *api, MC_METADATA *md, char *value, int value_sz, char *key, int key_sz) {
	/* assumption -> more free space after value, see __reserved_for_cas */
	int ret = api->get(api->storage_data, value, value_sz, key, key_sz);
	if(ret < (int)sizeof(MC_METADATA))
		return(-1);
	ret -= sizeof(MC_METADATA);
	assert(ret + (int)sizeof(MC_METADATA) <= value_sz);
	if(md)
		memcpy(md, value+ret, sizeof(MC_METADATA));
	return(ret);
}

int storage_set(ST_STORAGE_API *api, MC_METADATA *md, char *value, int value_sz, char *key, int key_sz) {
	/* assumption -> more free space after value, see __reserved_for_cas */
	char prev[sizeof(MC_METADATA)];
	
	/*save contents */
	memcpy(prev, value+value_sz, sizeof(MC_METADATA));
	
	/* put new metadata there */
	if(md)
		memcpy(value+value_sz, md, sizeof(MC_METADATA));
	else
		memset(value+value_sz, 0, sizeof(MC_METADATA));
	int ret = api->set(api->storage_data, value, value_sz + sizeof(MC_METADATA), key, key_sz);
	
	/*restore contents */
	memcpy(value+value_sz, prev, sizeof(MC_METADATA));
	
	return(ret);
}

int storage_delete(ST_STORAGE_API *api, char *key, int key_sz) {
	int ret = api->del(api->storage_data, key, key_sz);
	return(ret);
}

void storage_sync(ST_STORAGE_API *api) {
	if(api->sync)
		api->sync(api->storage_data);
}

void storage_prefetch(ST_STORAGE_API *api, char **keys, int *key_sz, int items_counter) {
	if(api->prefetch)
		api->prefetch(api->storage_data, keys, key_sz, items_counter);
}

