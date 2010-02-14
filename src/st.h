#ifndef _ST_H
#define _ST_H


#define VERSION_STRING "smalltable-0.0.1"

#include "storage.h"
#include "command.h"
#include "sys_commands.h"
#include "code_commands.h"


struct config {
	ST_STORAGE_API *api;
	
	char *tmpdir;
	
	int vx32_disabled;
	int syscall_limit;
	char *vx32sdk_gcc_command;
	char *vx32sdk_path;
};

#define CONFIG(conn) ((struct config*)(conn)->server->userdata)


#endif //_ST_H
