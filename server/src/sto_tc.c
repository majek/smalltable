#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st.h"

#include <tcutil.h>
#include <tchdb.h>
#include <stdbool.h>

struct tc_data{
	char *db_file;
	TCHDB *hdb;
};
typedef struct tc_data TC_DATA;

extern const char *tcversion;

// -1 -> not exists
int tc_get(void *storage_data, char *dst, int size, char *key, int key_sz) {
	TC_DATA *tcd = (TC_DATA *)storage_data;
	int ret = tchdbget3(tcd->hdb, key, key_sz, dst, size);
	return(ret);
}

// -1 -> not exits
// else: removed
int tc_del(void *storage_data, char *key, int key_sz) {
	TC_DATA *tcd = (TC_DATA *)storage_data;
	if( tchdbout(tcd->hdb, key, key_sz) )
		return(0);
	return(-1);
}

// written bytes
// -1 -> not saved
int tc_set(void *storage_data, char *value, int value_sz, char *key, int key_sz) {
	TC_DATA *tcd = (TC_DATA *)storage_data;
	int ret = tchdbput(tcd->hdb, key, key_sz, value, value_sz);
	if(ret)
		return(value_sz);
	return(-1);
}

void tc_sync(void *storage_data) {
	TC_DATA *tcd = (TC_DATA *)storage_data;
	tchdbsync(tcd->hdb);
}


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

