#ifndef _PROXY_H
#define _PROXY_H

#define VERSION_STRING "smalltable-proxy-0.0.1"

#include "shared.h"
#include "rbtree.h"

struct st_proxy;
struct st_server;

#include "st_server.h"
#include "st_proxy.h"
#include "proxy_command.h"
#include "proxy_sys_command.h"
#include "proxy_client.h"
#include "proxy_config.h"

struct st_proxy {
	struct rb_node node;
	
	char *host;
	int port;
};

struct st_server {
	struct rb_node node;
	
	char *host;
	int port;
	
	int weight;
	char *key;
	int key_sz;
	
	int sd;
	struct timespec next_retry;
	long long delay; // in nsec
	
	int requests;
	int send_offset;
	struct buffer send_buf;

	int responses;
	int recv_offset;
	struct buffer recv_buf;
};

struct config {
	char *config_path;
	char *config_path_new;

	struct rb_root proxies;
	struct rb_root servers;
	
	struct buffer res_buf;
};

#endif // _PROXY_H
