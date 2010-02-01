#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared.h"
#include "st.h"
#include "process.h"

struct process *find_process(char *key, int key_sz) {
	int cmd;
	for(cmd=0; cmd<256; cmd++) {
		int flags;
		struct process *process;
		command_get(cmd, &flags, (void*)&process);
		if(NULL == process)
			continue;
		if(process->key_sz == key_sz && !memcmp(process->key, key, key_sz))
			return(process);
	}
	return(NULL);
}

int process_compile(struct config *config, char *c_file, char *elf_file, char *dst, int dst_sz, int *produced_ptr) {
	int produced = 0;
	char log_file[1024];
	snprintf(log_file, sizeof(log_file), "%s.log", elf_file);
	
	unlink(elf_file);
	char cmd[4096];
	snprintf(cmd, sizeof(cmd),
			config->vx32sdk_gcc_command,
			config->vx32sdk_path, elf_file, c_file, log_file);
	int r = system(cmd);
	
	produced += snprintf(dst+produced, dst_sz-produced, "%s\n\n",cmd);
	
	int fd = open(log_file, O_RDONLY);
	if(fd >= 0) {
		produced += MAX(0, read(fd, dst+produced, dst_sz-produced));
		close(fd);
		unlink(log_file);
	}
	
	*produced_ptr = produced;
	return(r);
}

int write_file(char *fname, char *value, int value_sz) {
	int fd = open(fname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
	if(NEVER(fd < 0)) {
		return(-1);
	}
	
	int r = write(fd, value, value_sz);
	r = r;
	if(NEVER(r != value_sz)) {
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return(-1);
	}
	fsync(fd);
	close(fd);
	return(0);
}

int process_test_str(struct config *config, char *value, int value_sz, char *keyprefix) {
	int r;
	char c_file[1024];
	char elf_file[1024];
	snprintf(c_file, sizeof(c_file), "%s/plugin-%s.c", config->tmpdir, keyprefix);
	snprintf(elf_file, sizeof(elf_file), "%s/plugin-%s.elf", config->tmpdir, keyprefix);

	if(0 != write_file(c_file, value, value_sz)) {
		log_perror("write()");
		return(-1);
	}
	char warn_buf[4096];
	int warn_buf_sz = 0;
	if(0 != process_compile(config, c_file, elf_file, warn_buf, sizeof(warn_buf), &warn_buf_sz)) {
		warn_buf[MAX(1, warn_buf_sz)-1] = '\0';
		log_error("compilation failed:\n%s", warn_buf);
		return(-1);
	}
	struct process *process = process_new(NULL, keyprefix, strlen(keyprefix));
	r = process_load(process, elf_file);
	if(0 != r) {
		process_free(process);
		log_error("unable to load process:\n%s", warn_buf);
		return(-1);
	}
	r = process_run(NULL, process);
	process_free(process);
	if(r <= 0) {
		log_error("error while running process");
		return(-1);
	}
	// ok!
	return(r);
}

/*
   Request:
      MUST NOT have extras.
      MUST have key.
      MUST have value.
*/
ST_RES *cmd_code_load(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || !req->key_sz || !req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
	
	if(NULL != find_process(req->key, req->key_sz))
		return(set_error_code(res, MEMCACHE_STATUS_KEY_EXISTS, NULL));
	
	int r;
	char c_file[1024];
	char elf_file[1024];
	char keyprefix[32];
	key_escape(keyprefix, sizeof(keyprefix), req->key, req->key_sz);
	keyprefix[MIN(req->key_sz, sizeof(keyprefix)-1)] = '\0'; // make it reasonably short
	snprintf(c_file, sizeof(c_file), "%s/plugin-%s.c", CONFIG(conn)->tmpdir, keyprefix);
	snprintf(elf_file, sizeof(elf_file), "%s/plugin-%s.elf", CONFIG(conn)->tmpdir, keyprefix);
	
	res->value = res->buf;
	
	r = write_file(c_file, req->value, req->value_sz);
	if(NEVER(0 != r)) {
		res->status = MEMCACHE_STATUS_ITEM_NOT_STORED;
		res->value_sz += snprintf(&res->value[res->value_sz], res->buf_sz - res->value_sz, "Error while saving file: %s", strerror(errno));
		return(res);
	}
	
	if(0 != process_compile(CONFIG(conn), c_file, elf_file, res->value, res->buf_sz, (int*)&res->value_sz)) {
		res->status = MEMCACHE_STATUS_ITEM_NOT_STORED;
		return(res);
	}
	res->value_sz = MIN(res->value_sz, 4096); // no more than 4k logs
	res->value[res->value_sz++] = '\n';
	res->value[res->value_sz++] = '\n';
	
	struct process *process = process_new(conn, req->key, req->key_sz);
	r = process_load(process, elf_file);
	if(NEVER(0 != r)) {
		process_free(process);
		res->value_sz += snprintf(&res->value[res->value_sz], res->buf_sz - res->value_sz, "Error while loading the binary");
		res->status = MEMCACHE_STATUS_ITEM_NOT_STORED;
		return(res);
	}
	if(0 != process_run(conn, process)) {
		process_free(process);
		res->value_sz += snprintf(&res->value[res->value_sz], res->buf_sz - res->value_sz, "Error while running the binary");
		res->status = MEMCACHE_STATUS_ITEM_NOT_STORED;
		return(res);
	}
	res->status = MEMCACHE_STATUS_OK;
	return res;
}

/*
   Request:
      MUST NOT have extras.
      MUST have key.
      MUST NOT have value.
*/
ST_RES *cmd_code_unload(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || !req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
	
	struct process *process = find_process(req->key, req->key_sz);
	if(NULL == process)
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND, NULL));
	
	process_free(process);
	
	res->status = MEMCACHE_STATUS_OK;
	return res;
}

/*
   Request:
      MUST NOT have extras.
      MUST have key.
      MUST NOT have value.
*/
ST_RES *cmd_code_check(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || !req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
	
	struct process *process = find_process(req->key, req->key_sz);
	if(NULL == process)
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND, NULL));
	
	return(set_error_code(res, MEMCACHE_STATUS_KEY_EXISTS, NULL));
}

