#include "framing.h" // for MAX_KEY_SZ

struct mc_metadata{
	uint32_t flags;
	uint32_t expiration;
	uint64_t cas;
};
typedef struct mc_metadata MC_METADATA;


struct st_storage_api {
	int (*get)(void *storage_data, char *dst, int size, char *key, int key_sz);
	int (*set)(void *storage_data, char *value, int value_sz, char *key, int key_sz);
	int (*del)(void *storage_data, char *key, int key_sz);
	void (*sync)(void *storage_data);
	void (*readahead)(void *storage_data, char **keys, int *key_sz, int items_counter);
	void *storage_data;
};
typedef struct st_storage_api ST_STORAGE_API;


int storage_get(ST_STORAGE_API *api, MC_METADATA *md, char *value, int value_sz, char *key, int key_sz);
int storage_set(ST_STORAGE_API *api, MC_METADATA *md, char *value, int value_sz, char *key, int key_sz);
int storage_delete(ST_STORAGE_API *api, char *key, int key_sz);
void storage_prefetch(ST_STORAGE_API *api, char **keys, int *key_sz, int items_counter);
void storage_sync(ST_STORAGE_API *api);



typedef ST_STORAGE_API * (storage_engine_create)(int argc, char *argv[]);
typedef void (storage_engine_destroy)(ST_STORAGE_API *api);

storage_engine_create storage_fs_create;
storage_engine_destroy storage_fs_destroy;

storage_engine_create storage_dumb_create;
storage_engine_destroy storage_dumb_destroy;

#ifdef CONFIG_USE_TOKYOCABINET
storage_engine_create storage_tc_create;
storage_engine_destroy storage_tc_destroy;
#endif
#ifdef CONFIG_USE_BERKELEYDB
storage_engine_create storage_bdb_create;
storage_engine_destroy storage_bdb_destroy;
#endif
#ifdef CONFIG_USE_YDB
storage_engine_create storage_ydb_create;
storage_engine_destroy storage_ydb_destroy;
#endif


