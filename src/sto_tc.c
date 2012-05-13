#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include "shared.h"
#include "storage.h"

#include <tcutil.h>
#include <tchdb.h>
#include <stdbool.h>

char *encode_str(char *dst_start, const char *src, int src_sz)
{
	char tohex[] = "0123456789abcdef";
	char *dst = dst_start;
	int i;
	for(i=0; i<src_sz; i++) {
		unsigned char c = (unsigned char)src[i];
		if (isprint(c) && !isspace(c) && c != '\\' && c != '/') {
			*dst++ = c;
		} else {
			*dst++ = '\\';
			*dst++ = 'x';
			*dst++ = tohex[(c >> 4) & 0xF];
			*dst++ = tohex[(c) & 0xF];
		}
	}
	*dst++ = '\0';
	return dst_start;
}


void do_print(char *type, char *key, int key_sz, char *value, int value_sz)
{
	char *nk = NULL;
	if (key_sz > 0) {
		nk = alloca(key_sz * 4 + 1);
		encode_str(nk, key, key_sz);
	}
	char *nv = NULL;
	if (value_sz > 0) {
		nv = alloca(value_sz * 4 + 1);
		encode_str(nv, value, value_sz);
	}

	printf("%s %s %s\n", type, nk ?: "", nv ?: "");
}



struct tc_data{
	char *db_file;
	TCHDB *hdb;
};
typedef struct tc_data TC_DATA;

extern const char *tcversion;

// -1 -> not exists
int tc_get(void *storage_data, char *dst, int size, char *key, int key_sz) {
	do_print("GET", key, key_sz, NULL, 0);
	TC_DATA *tcd = (TC_DATA *)storage_data;
	int ret = tchdbget3(tcd->hdb, key, key_sz, dst, size);
	return(ret);
}

// -1 -> not exits
// else: removed
int tc_del(void *storage_data, char *key, int key_sz) {
	do_print("DEL", key, key_sz, NULL, 0);
	TC_DATA *tcd = (TC_DATA *)storage_data;
	if( tchdbout(tcd->hdb, key, key_sz) )
		return(0);
	return(-1);
}

// written bytes
// -1 -> not saved
int tc_set(void *storage_data, char *value, int value_sz, char *key, int key_sz) {
	do_print("SET", key, key_sz, value, value_sz);
	TC_DATA *tcd = (TC_DATA *)storage_data;
	int ret = tchdbput(tcd->hdb, key, key_sz, value, value_sz);
	if(ret)
		return(value_sz);
	return(-1);
}

void tc_sync(void *storage_data) {
	do_print("SYNC", NULL, 0, NULL, 0);
	TC_DATA *tcd = (TC_DATA *)storage_data;
	tchdbsync(tcd->hdb);
}

/*
// Not able to get reasonable metadata from tc.
static int tc_get_keys(void *storage_data, ) {
	TC_DATA *tcd = (TC_DATA *)storage_data;
	tchdbiterinit(tcd->hdb);
	while(1) {
		int key_sz;
		char *key;
		key = tchdbiternext(tcd->hdb, &key_sz);
		if(NULL == key) {
			break;
		}
		//value = tchdbget3(hdb, key);
	}
}
*/

ST_STORAGE_API *storage_tc_create(int argc, char **argv) {
	if(argc < 1)
		fatal("Please specify database file for TokyoCabinet.");
	if(argc > 1)
		fatal("Only one directory parameter allowed.");
	char *db_file = argv[0];
	
	log_info("TC storage database '%s', with lib version '%s'", db_file, tcversion);
	
	TC_DATA *tcd = (TC_DATA *)calloc(1, sizeof(TC_DATA));
	tcd->db_file = strdup(db_file);
	tcd->hdb = tchdbnew();
	/* http://www.dmo.ca/blog/benchmarking-hash-databases-on-large-data/*/
	tchdbsetcache(tcd->hdb, 12000000);
	
	if(!tchdbopen(tcd->hdb, db_file, HDBOWRITER | HDBOCREAT)) {
		int ecode = tchdbecode(tcd->hdb);
		log_perror("tchdbopen()= %s\n", tchdberrmsg(ecode));
		fatal("Died due to TokyoCabinet!");
	}

	ST_STORAGE_API *api = (ST_STORAGE_API *)calloc(1, sizeof(ST_STORAGE_API));
	api->get = &tc_get;
	api->set = &tc_set;
	api->del = &tc_del;
	api->sync = &tc_sync;
	api->storage_data = tcd;
	return(api);
}

void storage_tc_destroy(ST_STORAGE_API *api) {
	TC_DATA *tcd = (TC_DATA *)api->storage_data;
	
	tchdbsync(tcd->hdb);
	tchdbclose(tcd->hdb);
	tchdbdel(tcd->hdb);
	
	free(tcd->db_file);
	free(tcd);
	free(api);
}

