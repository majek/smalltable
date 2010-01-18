#ifndef _ROUTER_H
#define _ROUTER_H

#include <sys/types.h>
#include <event.h>

#include "coverage.h"

/* derived from libevent */
typedef void (*ev_callback_t)(int,short,void*);


#define VERSION_STRING "smalltable-0.0.1"

struct server;

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
	char trace;
	char ping_parent;

	char *host;
	int port;

	ST_STORAGE_API *api;
	
	char *tmpdir;
	
	int syscall_limit;
	char *vx32sdk_gcc_command;
	char *vx32sdk_path;
};



#endif //_ROUTER_H
