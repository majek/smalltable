#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"
#include "st.h"

#if __LP64__ == 1
# define PRINTF64U "l"
#else
# define PRINTF64U "ll"
#endif

u_int64_t unique_number;

#define SYSTEM_CMD(cmd, ptr, flag) \
	[cmd]		={(void*)ptr, CMD_FLAG_RO | flag}

#define PROCESS_CMD(cmd, ptr, flag) \
	[cmd]		={(void*)ptr, CMD_FLAG_RO | CMD_FLAG_EXTRA_PARAM | flag}

struct cmd_pointers cmd_pointers[256] = {
	SYSTEM_CMD(MEMCACHE_CMD_GET, &cmd_get, CMD_FLAG_PREFETCH),
	SYSTEM_CMD(MEMCACHE_CMD_ADD, &cmd_set, CMD_FLAG_PREFETCH),
	SYSTEM_CMD(MEMCACHE_CMD_SET, &cmd_set, 0),
	SYSTEM_CMD(MEMCACHE_CMD_REPLACE, &cmd_set, CMD_FLAG_PREFETCH),
	SYSTEM_CMD(MEMCACHE_CMD_DELETE, &cmd_delete, 0),
	SYSTEM_CMD(MEMCACHE_CMD_NOOP, &cmd_noop, 0),
	SYSTEM_CMD(MEMCACHE_CMD_VERSION, &cmd_version, 0),
	PROCESS_CMD(MEMCACHE_XCMD_CODE_LOAD, &cmd_code_load, 0),
	PROCESS_CMD(MEMCACHE_XCMD_CODE_UNLOAD, &cmd_code_unload, 0),
	PROCESS_CMD(MEMCACHE_XCMD_CODE_CHECK, &cmd_code_check, 0),
	SYSTEM_CMD(MEMCACHE_XCMD_GET_KEYS, &cmd_get_keys, 0),
};

int command_register(int cmd, int user_flags, void *process_ud) {
	if(cmd < 0 || cmd > NELEM(cmd_pointers) || (cmd_pointers[cmd].flags & CMD_FLAG_RO))
		return(-1);
	int flags = CMD_FLAG_PROCESS | (user_flags & (CMD_FLAG_PREFETCH));
	cmd_pointers[cmd].ptr = process_ud;
	cmd_pointers[cmd].flags = flags;
	return(0);
}

int command_unregister(int cmd) {
	if(cmd < 0 || cmd > NELEM(cmd_pointers) || (cmd_pointers[cmd].flags & CMD_FLAG_RO)) { // no coverage
		return(-1);
	}
	cmd_pointers[cmd].ptr = NULL;
	cmd_pointers[cmd].flags = 0;
	return(0);
}

void command_get(int cmd, int *flags_ptr, void **process_ud) {
	if(cmd < 0 || cmd > NELEM(cmd_pointers) || (cmd_pointers[cmd].flags & CMD_FLAG_RO)) {
		*process_ud = NULL;
		*flags_ptr = 0;
		return;
	}
	*process_ud = cmd_pointers[cmd].ptr;
	*flags_ptr = cmd_pointers[cmd].flags;
}

int command_find_by_process_ud(void *process_ud) {
	int cmd;
	for(cmd=0; cmd < NELEM(cmd_pointers); cmd++) {
		if(cmd_pointers[cmd].ptr == process_ud)
			return(cmd);
	}
	return(-1);
}

static void try_prefetch(ST_STORAGE_API *api, char *start_req_buf, int start_req_buf_sz) {
	#define BULK_SZ 4096
	int b_sz = 0;
	static char *b_key[BULK_SZ]; // no thread safe
	static int b_key_sz[BULK_SZ];

	int first_run = 1;

	char *req_buf = start_req_buf;
	char *end_req_buf = start_req_buf + start_req_buf_sz;
	while(end_req_buf-req_buf) {
		int request_sz = MC_GET_REQUEST_SZ(req_buf);
		int cmd = MC_GET_OPCODE(req_buf);
		int flags = cmd_pointers[cmd].flags;
		if(flags & CMD_FLAG_PREFETCH) {
			if(unlikely(first_run)) {
				/* clear previous prefetches */
				storage_prefetch(api, NULL, NULL, 0);
				first_run = 0;
			}
			REQBUF_GET_KEY(req_buf, &b_key[b_sz], &b_key_sz[b_sz]);
			b_sz += 1;
			if(unlikely(b_sz == BULK_SZ)) {
				storage_prefetch(api, b_key, b_key_sz, b_sz);
				b_sz = 0;
			}
		}
		req_buf += request_sz;
	}
	if(b_sz) {
		storage_prefetch(api, b_key, b_key_sz, b_sz);
	}
}



/* Enough data is collected. Do the magic - dispatch commands.
   Pop requests from req_buf, write replies to send_buf. 
   Return: consumed bytes. */
int process_multi(CONN *conn, char *start_req_buf, int start_req_buf_sz) {
	/* This is a weird hack. Actually, when command is doing api->set,
	   the set requires to append some metadata to the end of value.
	   So, we'd better make sure that we have enough allocated space after
	   the end of the read data. */
	buf_get_writer(&conn->recv_buf, NULL, NULL, sizeof(MC_METADATA) );

	try_prefetch(CONFIG(conn)->api, start_req_buf, start_req_buf_sz);
	
	char *req_buf = start_req_buf;
	int req_buf_sz = start_req_buf_sz;
	char *end_req_buf = req_buf + req_buf_sz;
	
	/* Group commands and run callback for that group. */
	void *group_ptr = (void*) -1; // NULL in ptr is absolutely valid
	char *group_req_buf = req_buf;
	int group_flags = -1;
	int group_req_buf_sz = 0;
	commands_callback_ptr group_fun = NULL;
	while(end_req_buf-req_buf) {
		int request_sz = MC_GET_REQUEST_SZ(req_buf);

		int cmd = MC_GET_OPCODE(req_buf);
		void *ptr = cmd_pointers[cmd].ptr;
		int flags = cmd_pointers[cmd].flags;
		if(group_ptr == ptr && group_flags == flags) { /* swallow */
			if(conn->server->trace) { // never
				log_info("%s:%i             cmd=0x%02x(%i)", conn->host, conn->port, cmd, cmd);
			}
			// pass;
		} else {
			if(likely(group_req_buf_sz)) {
				/* run callback for previous group */
				group_fun(conn,
					group_req_buf, group_req_buf_sz,
					&conn->send_buf, group_flags, group_ptr);
			}
			if(conn->server->trace) { // never
				log_info("%s:%i flags=0x%x(%i) ptr=%p cmd=0x%02x(%i)", conn->host, conn->port, flags, flags, ptr, cmd, cmd);
			}
			group_req_buf = req_buf;
			group_req_buf_sz = 0;
			group_ptr = ptr;
			group_flags = flags;
			if(!(flags & CMD_FLAG_PROCESS)) { // normal
				group_fun = builtin_commands_callback;
			} else { // process
				group_fun = process_commands_callback;
			}
		}
		req_buf += request_sz;
		group_req_buf_sz += request_sz;
	}
	if(likely(group_req_buf_sz)) {
		group_fun(conn,
			group_req_buf, group_req_buf_sz,
			&conn->send_buf, group_flags, group_ptr);
	}
	return(req_buf - start_req_buf);
}




void commands_initialize() {
	int fd = open("/dev/urandom", O_RDONLY);
	if(fd >= 0) { // always
		int r = read(fd, &unique_number, sizeof(unique_number));
		r = r;
		if(r != sizeof(unique_number)) { // never
			fd = -1;
		}
		close(fd);
	}
	if(fd < 0) { // never
		log_error("Can't read from /dev/urandom. Starting with reduced randomnes%c.", 's');
		srand(time(NULL));
		unique_number = ((u_int64_t)rand() << 32) || ((u_int64_t)rand());
	}
	log_info("Starting with unique: 0x%016" PRINTF64U "x", unique_number);
}

void commands_destroy() {
	;
}

