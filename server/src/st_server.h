
#define MAX_SERVERS 1024
/* 100msec */
#define ST_SERVER_MIN_DELAY (100000000LL)
/* 5sec */
#define ST_SERVER_MAX_DELAY (5000000000LL)

int new_st_server(struct config *config, char *host, int port, int weight, char *key, int key_sz);
void del_st_server(struct config *config, struct st_server *srv);
struct st_server *find_st_server(struct config *config, char *key, int key_sz);
void free_all_servers(struct config *config);



