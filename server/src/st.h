#ifndef _ST_H
#define _ST_H

#include <sys/types.h>
#include <event.h>

#include "coverage.h"

/* derived from libevent */
typedef void (*ev_callback_t)(int,short,void*);


#define VERSION_STRING "smalltable-0.0.1"

struct server;
struct config;

#include "common.h"
#include "buffer.h"
#include "event_loop.h"
#include "network.h"

#include "connection.h"
#include "framing.h"

#include "storage.h"


#include "command.h"
#include "sys_commands.h"
#include "code_commands.h"


struct server {
	int sd;
	struct event ev;

	char *host;
	int port;

	char ping_parent;
	
	void (*info_handler)(void *userdata);
	void (*quit_handler)(void *userdata);
	int (*process_multi)(CONN *conn, char *requests, int requests_sz);
	void *userdata;
};

struct config {
	char trace;
	ST_STORAGE_API *api;
	
	char *tmpdir;
	
	int syscall_limit;
	char *vx32sdk_gcc_command;
	char *vx32sdk_path;
};

#define CONFIG(conn) ((struct config*)(conn)->server->userdata)


#endif //_ST_H
