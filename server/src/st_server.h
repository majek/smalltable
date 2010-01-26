
#define MAX_SERVERS 1024

int new_st_server(struct config *config, char *host, int port, int weight, char *key, int key_sz);
void del_st_server(struct config *config, struct st_server *srv);
struct st_server *find_st_server(struct config *config, char *key, int key_sz);



