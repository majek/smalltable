
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "libvxc/syscall.h"
#include "smalltable.h"

void __exit(int status) {
	while(1)
		syscall(VXSYSEXIT, status, 0, 0, 0, 0);
}


/* Register this process to serve specific command.
	-1 - register failed
	0 - registered with success
*/
int st_register(int cmd, int cmd_flags) {
	return (int)syscall(VXSYSREGISTER, cmd, cmd_flags, 0, 0, 0);
}

/* Unregister command from this process.
	-1 - unregister failed
	0 - unregistered
*/
int st_unregister(int cmd) {
	return (int)syscall(VXSYSUNREGISTER, cmd, 0, 0, 0, 0);
}

/* Wait for commands. Put requests into the buffer.
	x - filled buffer_size
*/
int st_read_requests(char *buf, int buf_sz) {
	return (int)syscall(VXSYSREADREQUESTS, (int)buf, buf_sz, 0, 0, 0);
}

/* 
*/
int st_write_responses(char *buf, int buf_sz) {
	return (int)syscall(VXSYSWRITERESPONSES, (int)buf, buf_sz, 0, 0, 0);
}

int syscall_st_get(char *dst, int dst_sz, char *key, int key_sz) {
	return (int)syscall(VXSYSGET, (int)dst, dst_sz, (int)key, key_sz, 0);
}

int syscall_st_prefetch(char **keys, int *keys_sz, int items_counter) {
	return (int)syscall(VXSYSPREFETCH, (int)keys, (int)keys_sz, items_counter, 0, 0);
}

int syscall_st_set(char *value, int value_sz, char *key, int key_sz) {
	return (int)syscall(VXSYSSET, (int)value, value_sz, (int)key, key_sz, 0);
}

int syscall_st_del(char *key, int key_sz) {
	return (int)syscall(VXSYSDEL, (int)key, key_sz, 0, 0, 0);
}

int st_get_random(char *dst, int dst_sz) {
	return (int)syscall(VXSYSGETRANDOM, (int)dst, dst_sz, 0, 0, 0);
}



int unpack_request(ST_REQ *req, char *buf, int buf_sz) {
	struct memcache_header *header = (struct memcache_header *)buf;
	req->opcode = header->opcode; // 8bit
	req->status = ntohs(header->status); // 16bit
	req->opaque = ntohl(header->opaque); // 32bit
	req->cas = ntohll(header->cas);	// 64 bit
	
	uint32_t extras_length = header->extras_length; // 8bit
	uint32_t key_length = ntohs(header->key_length); // 16 bit
	uint32_t body_length = ntohl(header->body_length); // 32bit

	req->extras_sz	= extras_length;
	if(extras_length)
		req->extras = buf + MEMCACHE_HEADER_SIZE;
	else
		req->extras = NULL;

	req->key_sz	= key_length;
	if(key_length)
		req->key = buf + MEMCACHE_HEADER_SIZE + extras_length;
	else
		req->key = NULL;
	
	req->value	= buf + MEMCACHE_HEADER_SIZE + extras_length + key_length;
	req->value_sz	= body_length - extras_length - key_length;
	return(0);
}


int pack_response(char *buf, int buf_sz, ST_RES *res) {
	int body_length = res->extras_sz + res->key_sz + res->value_sz;
	if(buf_sz < MEMCACHE_HEADER_SIZE + body_length) {
		fatal("broken req buffer");
	}

	struct memcache_header *header = (struct memcache_header *)buf;
	header->magic = 0x81;
	header->opcode = res->opcode; // 8bit
	header->key_length = htons(res->key_sz); // 16bit
	header->extras_length = res->extras_sz; //8bit
	header->data_type = 0x0;
	header->status = htons(res->status); // 16bi
	header->body_length = htonl(body_length); // 32bit
	header->opaque = htonl(res->opaque); // 32 bit
	header->cas = htonll(res->cas); // 64bit
	
	if(0 != res->extras_sz) {
		if(buf + MEMCACHE_HEADER_SIZE != res->extras) {
			fatal("bad extras pointer");
		}
	}
	if(0 != res->key_sz) {
		if(buf + MEMCACHE_HEADER_SIZE + res->extras_sz != res->key) {
			fatal("bad key pointer");
		}
	}
	if(0 != res->value_sz) {
		if(buf + MEMCACHE_HEADER_SIZE + res->extras_sz + res->key_sz != res->value) {
			fatal("bad value pointer");
		}
	}
	return(MEMCACHE_HEADER_SIZE + body_length);
}



char *error_codes[] = {
	[MEMCACHE_STATUS_KEY_NOT_FOUND]		"Key not found",
	[MEMCACHE_STATUS_KEY_EXISTS]		"Key exists",
	[MEMCACHE_STATUS_VALUE_TOO_BIG]		"Value too big",
	[MEMCACHE_STATUS_INVALID_ARGUMENTS]	"Invalid arguments",
	[MEMCACHE_STATUS_ITEM_NOT_STORED]	"Item not stored",
	[MEMCACHE_STATUS_UNKNOWN_COMMAND]	"Unknown command"
};


ST_RES *set_error_code(ST_RES *res, int status) {
	res->extras_sz = 0;
	res->key_sz = 0;
	res->cas = 0;

	res->status = status;
	res->value = res->buf;
	
	char *str = "Unknown error code";
	if(status >= 0 && status< NELEM(error_codes))
		str = error_codes[status];
	res->value_sz = MIN(strlen(str), res->buf_sz);
	memcpy(res->value, str, res->value_sz);
	return(res);
}



int storage_get(MC_METADATA *md, char *value, int value_sz, char *key, int key_sz) {
	int ret = syscall_st_get(value, value_sz, key, key_sz);
	if(ret < (int)sizeof(MC_METADATA))
		return(-1);
	ret -= sizeof(MC_METADATA);
	if(md)
		memcpy(md, value+ret, sizeof(MC_METADATA));
	return(ret);
}

int storage_set(MC_METADATA *md, char *value, int value_sz, char *key, int key_sz) {
	/* assumption -> more free space after value, to fit MC_METADATA there */
	static char prev[sizeof(MC_METADATA)];
	
	/* save contents of value memory, above value_sz! */
	memcpy(prev, value+value_sz, sizeof(MC_METADATA));
	
	/* put contents there */
	if(md)
		memcpy(value+value_sz, md, sizeof(MC_METADATA));
	else
		memset(value+value_sz, 0, sizeof(MC_METADATA));
	int ret = syscall_st_set(value, value_sz + sizeof(MC_METADATA), key, key_sz);
	
	/*restore contents */
	memcpy(value+value_sz, prev, sizeof(MC_METADATA));
	
	return(ret);
}

int storage_delete(char *key, int key_sz) {
	return(syscall_st_del(key, key_sz));
}

void storage_prefetch(char **keys, int *key_sz, int items_counter) {
	syscall_st_prefetch(keys, key_sz, items_counter);
}

void fatal(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fflush(stdout);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\nQUITTING!\n");
	va_end(ap);
	__exit(0xDEAD);
}

static int process_single_command(struct commands_pointers *cmd_ptr, int cmd_ptr_sz,
				char *request, int request_sz,
				char *res_buf, int res_buf_sz) {
	ST_REQ req;
	int r = unpack_request(&req, request, request_sz);
	
	ST_RES res;
	memset(&res, 0, sizeof(res));
	res.opcode = req.opcode;
	res.opaque = req.opaque;
	res.buf = res_buf + MEMCACHE_HEADER_SIZE;
	res.buf_sz = res_buf_sz - MEMCACHE_HEADER_SIZE;
	if(r < 0) {
		set_error_code(&res, MEMCACHE_STATUS_INVALID_ARGUMENTS);
		goto exit;
	}
	
	int cmd = req.opcode;
	if(cmd >= 0 && cmd < cmd_ptr_sz && cmd_ptr[cmd].fun) {
		cmd_ptr[cmd].fun(&req, &res);
	} else {
		set_error_code(&res, MEMCACHE_STATUS_UNKNOWN_COMMAND);
	}

exit:;
	return( pack_response(res_buf, res_buf_sz, &res) );
}


uint64_t unique_number;

void simple_commands_loop(struct commands_pointers *cmd_ptr, int cmd_ptr_sz) {
	st_get_random((char*)&unique_number, sizeof(unique_number));

	fprintf(stderr, "Started with unique 0x%08llx\n", unique_number);

	int i;
	for(i=0; i < cmd_ptr_sz; i++) {
		if(cmd_ptr[i].fun) {
			if(0 != st_register(i, cmd_ptr[i].flags))
				fprintf(stderr, "Failed to register cmd 0x%x", i);
		}
	}

	char *req_buf = (char *)malloc(READ_REQUESTS_BUF_SZ);
	char *res_buf = (char *)malloc(WRITE_RESPONSES_BUF_SZ);
	while(1) {
		char *buf = req_buf;
		int buf_sz = st_read_requests(req_buf, READ_REQUESTS_BUF_SZ);
		if(buf_sz < 0)
			break;
		char *buf_end = buf + buf_sz;
		while(buf_end - buf) {
			int request_sz = MC_GET_REQUEST_SZ(buf);
			int res_sz = process_single_command(cmd_ptr, cmd_ptr_sz,
							buf, request_sz,
							res_buf, WRITE_RESPONSES_BUF_SZ);
			st_write_responses(res_buf, res_sz);
			buf += request_sz;
		}
	}
	free(req_buf);
	free(res_buf);
	return;
}
