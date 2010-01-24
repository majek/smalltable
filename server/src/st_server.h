
int new_st_server(struct config *config, char *host, int port, int weight, char *key, int key_sz);
void del_st_server(struct config *config, struct st_server *srv);
