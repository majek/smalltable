#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "proxy.h"


static int key_cmp(char *a, int a_sz, char *b, int b_sz) {
	int r = memcmp(a, b, MIN(a_sz, b_sz));
	if(r == 0) {
		if(a_sz == b_sz)
			return(0);
		if(a_sz < b_sz)
			return(-1);
		return(1);
	}
	return(r);
}


static int st_server_insert(struct config *config, struct st_server *data) {
	struct rb_node **new = &(config->servers.rb_node), *parent = NULL;

	while (*new) {
		struct st_server *item = container_of(*new, struct st_server, node);
		int result = key_cmp(data->key, data->key_sz, item->key, item->key_sz);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &config->servers);

	return 1;
}

static void _del_st_server(struct st_server *srv) {
	buf_free(&srv->send_buf);
	buf_free(&srv->recv_buf);
	free(srv->host);
	free(srv->key);
	free(srv);
}


int new_st_server(struct config *config, char *host, int port, int weight, char *key, int key_sz) {
	struct st_server *srv = (struct st_server *)calloc(1, sizeof(struct st_server));
	srv->host = strdup(host);
	srv->port = port;
	srv->weight = weight;
	srv->sd = -1;
	srv->delay = ST_SERVER_MIN_DELAY;
	if(key_sz) {
		srv->key = (char*)malloc(key_sz);
		memcpy(srv->key, key, key_sz);
	} else {
		srv->key = strdup("");
	}
	srv->key_sz = key_sz;
	clock_gettime(CLOCK_MONOTONIC, &srv->next_retry);
	
	int r = st_server_insert(config, srv);
	if(!r) {
		log_error("Duplicate st_server %s:%i", srv->host, srv->port);
		_del_st_server(srv);
		return(-1);
	}
	return(0);
}

void del_st_server(struct config *config, struct st_server *srv) {
	rb_erase(&srv->node, &config->servers);
	_del_st_server(srv);
}

/* Apparently it's way more than O(ln(N)). */
struct st_server *find_st_server(struct config *config, char *key, int key_sz) {
	struct rb_node *node = config->servers.rb_node;
	
	while(node) {
		struct st_server *item = container_of(node, struct st_server, node);
		int result = key_cmp(key, key_sz, item->key, item->key_sz);
	
		if (result < 0) {
			node = node->rb_left;
		} else { // >= 0
			struct rb_node *right = rb_next(node);
			if(!right) {
				return(container_of(node, struct st_server, node));
			} else {
				struct st_server *right_srv = container_of(right, struct st_server, node);
				if(key_cmp(key, key_sz, right_srv->key, right_srv->key_sz) < 0)
					return(container_of(node, struct st_server, node));
			}
			node = node->rb_right;
		}
	}
	log_error("Server not found. That's very bad.");
	return NULL;
}

void free_all_servers(struct config *config) {
	while(1) {
		struct rb_node *first_node = (config->servers.rb_node);
		if(NULL == first_node)
			break;
		struct st_server *server = container_of(first_node, struct st_server, node);
		del_st_server(config, server);
	}
}
