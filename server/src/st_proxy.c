#include <stdlib.h>
#include <string.h>

#include "proxy.h"

static int proxy_key_cmp(struct st_proxy *a, struct st_proxy *b) {
	int r = strcmp(a->host, b->host);
	if(r != 0)
		return(r);
	if(a->port < b->port)
		return(-1);
	if(a->port > b->port)
		return(1);
	return(0);
}

static int st_proxy_insert(struct config *config, struct st_proxy *data) {
	struct rb_node **new = &(config->proxies.rb_node), *parent = NULL;

	while (*new) {
		struct st_proxy *item = container_of(*new, struct st_proxy, node);
		int result = proxy_key_cmp(data, item);

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

static void _del_st_proxy(struct st_proxy *proxy) {
	free(proxy->host);
	free(proxy);
}


int new_st_proxy(struct config *config, char *host, int port) {
	struct st_proxy *proxy = (struct st_proxy *)calloc(1, sizeof(struct st_proxy));
	proxy->host = strdup(host);
	proxy->port = port;
	int r = st_proxy_insert(config, proxy);
	if(!r) {
		log_error("Duplicate st_proxy %s:%i", proxy->host, proxy->port);
		_del_st_proxy(proxy);
		return(-1);
	}
	return(0);
}

void del_st_proxy(struct config *config, struct st_proxy *proxy) {
	rb_erase(&proxy->node, &config->proxies);
	_del_st_proxy(proxy);
}
