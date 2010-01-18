#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "st.h"
#include "process.h"

// From ../untrusted/libst/smalltable.h
enum {
	VXSYSREGISTER = 0xFF00,
	VXSYSUNREGISTER,
	VXSYSREADREQUESTS,
	VXSYSWRITERESPONSES,
	VXSYSGET,
	VXSYSPREFETCH,
	VXSYSSET,
	VXSYSDEL,
	VXSYSGETRANDOM
};


extern int vx_elfbigmem;

void process_initialize(struct server *server) {
	vx_elfbigmem = 0;
	vx32_siginit();
	
	struct itimerval timer;
	/* Configure the timer to expire after 1000 msec... */
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 500*1000;
	/* ... and every 1000 msec after that. */
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 500*1000;
	/* Start a virtual timer. It counts down whenever this process is executing. */
	setitimer(ITIMER_VIRTUAL, &timer, NULL);
	
	/* Check if we can compile and load simple program */
	char *code =  \
		"#include <stdlib.h>\n"
		"#include <smalltable.h>\n"
		"int main() { __exit(112); }\n";
	
	if(112 != process_test_str(server, code, strlen(code), "testcode")) {
		fatal("Unable to run test code. Check your gcc and vx32sdk paths.");
	}
	return;
}

void process_destroy() {
	setitimer(ITIMER_VIRTUAL, NULL, NULL);
}

struct process *process_new(CONN *conn, char *key, int key_sz) {
	struct process *process = (struct process *)st_calloc(1, sizeof(struct process));
	process->key = (char*)st_malloc(key_sz);
	process->key_sz = key_sz;
	memcpy(process->key, key, key_sz);
	process->owner_host = strdup(conn ? conn->host : "unknown");
	process->owner_port = conn ? conn->port : 0;
	process->owner_time = time(NULL);
	
	char buf[32];
	key_escape(buf, sizeof(buf), key, key_sz);
	buf[MIN(key_sz, sizeof(buf)-1)] = '\0'; // make it reasonably short
	process->encoded_key = strdup(buf);
	return(process);
}

void process_free(struct process *process) {
	while(1) {
		int cmd = command_find_by_process_ud(process);
		if(cmd < 0)
			break;
		command_unregister(cmd);
		process->registercnt--;
	}
	if(NEVER(process->registercnt)) {
		log_error("reference counter != 0! %i", process->registercnt);
	}
	if(process->p) {
		vxproc_free(process->p);
		process->p = NULL;
	}

	free(process->key);
	free(process->owner_host);
	free(process->encoded_key);
	free(process);
}

void process_commands_callback(CONN *conn, char *req_buf, int req_buf_sz, struct buffer *send_buf, int cmd_flags, void *cmd_ptr) {
	struct process *process = (struct process *)cmd_ptr;
	
	if(process->user_buf_sz >= req_buf_sz) {
		memcpy(process->user_buf, req_buf, req_buf_sz);
	} else {
		log_warn("#%p request buffer too small! %i < %i", process, process->user_buf_sz, req_buf_sz);
		goto error;
	}
	
	process->send_buf = send_buf;
	process->send_sz = 0;
	
	process->p->cpu->reg[EAX] = req_buf_sz; /* return value */
	if(0 != process_run(conn, process)) {
		log_warn("#%p is dead after request from %s:%i", process, conn ? conn->host : "unknown", conn ? conn->port : 0);
		goto error;
	}
	
	if(process->send_sz == 0) {
		log_warn("#%p no reposne given", process);
		goto error;
	}
	if(conn && conn->server->trace)
		log_warn("#%p sending %i", process, process->send_sz);
	return;
	
error:;
	if(process->send_sz != 0) {
		/* reset buffer, don't send what the process given, as it can
		   be incomplete */
		buf_rollback_produced(send_buf, process->send_sz);
	}
	/* create error messages */
	char *end_req_buf = req_buf + req_buf_sz;
	while(end_req_buf-req_buf) {
		int request_sz = MC_GET_REQUEST_SZ(req_buf);
		
		char *res;
		int res_sz;
		buf_get_writer(send_buf, &res, &res_sz, MAX_REQUEST_SIZE);
		
		int produced = error_from_reqbuf(req_buf, request_sz, res, res_sz, MEMCACHE_STATUS_UNKNOWN_COMMAND);
		buf_produce(send_buf, produced);
		
		req_buf += request_sz;
	}
	process_free(process);
	return;
}



enum {
	SYSCALLRET_CONTINUE = 1,
	SYSCALLRET_YIELD = 2,
	SYSCALLRET_EXIT = 3,
	SYSCALLRET_KILL = 4
};

int syscall_exit(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	process->exit_code = arg1;
	*syscall_ret = SYSCALLRET_EXIT;
	return(0);
}

int syscall_brk(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t oaddr;
	uint32_t addr = arg1;
	uint32_t inc = 1<<20;
	addr = (addr + inc - 1) & ~(inc - 1);
	oaddr = m->size;
	if(addr == oaddr) {
		return(0);
	}
	int ret = 0;
	if(addr > m->size) {
		ret = vxmem_resize(proc->mem, addr);
		if(ret < 0) {
			log_warn("sbrk failed. caller will be unhappy!");
			*syscall_ret = SYSCALLRET_KILL;
			return(ret);
		}
	}
	if (ret >= 0) {
		if (addr > oaddr) {
			ret = vxmem_setperm(proc->mem, oaddr, addr - oaddr,
						      VXPERM_READ|VXPERM_WRITE);
			if(ret < 0) {
				log_warn("setperm is failing!");
				*syscall_ret = SYSCALLRET_KILL;
				return(ret);
			}
		}
	}
	return(ret);
}

int syscall_write(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t arg3 = proc->cpu->reg[EBX];
	int fd = arg1;
	uint32_t addr = arg2;
	int len = arg3;
	if(fd != 1 && fd != 2) {
		return(-EINVAL);
	}
	if (!vxmem_checkperm(proc->mem, addr, len, VXPERM_READ, NULL)) {
		return(-EINVAL);
	}
	char buf[4096];
	strncpy(buf, (char*)m->base + addr, MIN(sizeof(buf), len));
	buf[MIN(len, sizeof(buf)-1)] = '\0';
	if(strlen(buf)>0 && buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\0';
	if(!strlen(buf))
		return(0);
	log_warn("#%p : %s", process, buf);
	return(len);
}

int syscall_register(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	int cmd = arg1;
	int flags = arg2;
	
	if(cmd < 0 || cmd > 255)
		return(-1);
	
	flags &= (CMD_FLAG_QUIET|CMD_FLAG_PREFETCH);
	
	int xflags;
	void *process_ud;
	command_get(cmd, &xflags, &process_ud);
	if(process_ud && process_ud != process) {
		struct process *looser = (struct process*)process_ud;
		looser->registercnt--;
		if(0 == looser->registercnt) {
			log_info("#%p: unused - killing", looser);
			process_free(looser);
		}
	}
	
	int r = command_register(cmd, flags, (void *)process);
	if(0 == r) {
		process->registercnt++;
		log_info("#%p: registered cmd 0x%02x", process, cmd);
	}
	return(r);
}

int syscall_unregister(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	int cmd = arg1;
	
	if(cmd < 0 || cmd > 255)
		return(-1);
	int flags;
	void *userdata;
	command_get(cmd, &flags, &userdata);
	if(userdata != process)
		return(-1);
	log_info("#%p: unregistered cmd 0x%02x", process, cmd);
	command_unregister(cmd);
	process->registercnt--;
	return(0);
}

int syscall_readrequests(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	if(0 == process->registercnt) {
		log_warn("#%p not registered but wants to block", process);
		return(-1);
	}
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t addr = arg1;
	int len = arg2;
	if(!vxmem_checkperm(proc->mem, addr, len, VXPERM_WRITE, NULL)) {
		return(-EINVAL);
	}
	if(len < MAX_REQUEST_SIZE) {
		log_warn("#%p insufficient buffer. %i < %i", process, len, MAX_REQUEST_SIZE);
		*syscall_ret = SYSCALLRET_KILL;
		return(-EINVAL);
	}
	process->user_buf = (char*)m->base + addr;
	process->user_buf_sz = len;
	
	*syscall_ret = SYSCALLRET_YIELD;
	return(0);
}

int syscall_writeresponses(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t addr = arg1;
	int len = arg2;
	if(len < 1)
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, addr, len, VXPERM_READ, NULL)) {
		return(-EINVAL);
	}
	char *res_buf;
	buf_get_writer(process->send_buf, &res_buf, NULL, len);
	memcpy(res_buf, (char*)m->base + addr, len);
	buf_produce(process->send_buf, len);
	process->send_sz += len;
	
	if(process->send_sz >= (MAX_REQUEST_SIZE*3)) {
		log_warn("#%p trying to send too much", process);
		*syscall_ret = SYSCALLRET_KILL;
		return(-EINVAL);
	}
	return(len);
}

int syscall_st_get(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t arg3 = proc->cpu->reg[EBX];
	uint32_t arg4 = proc->cpu->reg[EDI];
	uint32_t dst_addr = arg1;
	int dst_sz = arg2;
	uint32_t key_addr = arg3;
	int key_sz = arg4;
	if(dst_sz < 0 || key_sz < 0)
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, dst_addr, dst_sz, VXPERM_WRITE, NULL))
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, key_addr, key_sz, VXPERM_READ, NULL))
		return(-EINVAL);
	
	char *dst = (char*)m->base + dst_addr;
	char *key = (char*)m->base + key_addr;
	
	ST_STORAGE_API *api = conn->server->api;
	return( api->get(api->storage_data, dst, dst_sz, key, key_sz) );
}

int syscall_st_prefetch(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	ST_STORAGE_API *api = conn->server->api;
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t arg3 = proc->cpu->reg[EBX];
	uint32_t keys_addr = arg1;
	uint32_t keys_sz_addr = arg2;
	int items_counter = arg3;
	
	if(items_counter < 0)
		return(-EINVAL);
	if(0 == keys_addr && 0 == keys_sz_addr && 0 == items_counter) {
		if(api->prefetch)
			api->prefetch(api->storage_data, NULL, NULL, 0);
		return(0);
	}
	if(!vxmem_checkperm(proc->mem, keys_addr, sizeof(char*)*items_counter, VXPERM_READ|VXPERM_WRITE, NULL))
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, keys_sz_addr, sizeof(int)*items_counter, VXPERM_READ, NULL))
		return(-EINVAL);

	char **keys = (char**)((char *)m->base + keys_addr);
	int *keys_sz = (int*)((char*)m->base + keys_sz_addr);

	if(api->prefetch) {
		int i;
		/* fix key pointers */
		for(i=0; i < items_counter; i++) {
			keys[i] = (char*)m->base + (long)keys[i];
		}
		api->prefetch(api->storage_data, keys, keys_sz, items_counter);
		/* clear pointers */
		memset(keys, 0, sizeof(keys[0])*items_counter);
	}
	return(0);
}

int syscall_st_set(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t arg3 = proc->cpu->reg[EBX];
	uint32_t arg4 = proc->cpu->reg[EDI];
	uint32_t value_addr = arg1;
	int value_sz = arg2;
	uint32_t key_addr = arg3;
	int key_sz = arg4;
	if(value_sz < 0 || key_sz < 0)
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, value_addr, value_sz, VXPERM_READ, NULL))
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, key_addr, key_sz, VXPERM_READ, NULL))
		return(-EINVAL);
		
	char *value = (char*)m->base + value_addr;
	char *key = (char*)m->base + key_addr;

	ST_STORAGE_API *api = conn->server->api;
	return( api->set(api->storage_data, value, value_sz, key, key_sz) );
}

int syscall_st_del(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t key_addr = arg1;
	int key_sz = arg2;
	if(key_sz < 0)
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, key_addr, key_sz, VXPERM_READ, NULL))
		return(-EINVAL);
	
	char *key = (char*)m->base + key_addr;
	
	ST_STORAGE_API *api = conn->server->api;
	return( api->del(api->storage_data, key, key_sz) );
}

int syscall_st_getrandom(CONN *conn, struct process *process, vxproc *proc, vxmmap *m, int *syscall_ret) {
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t dst_addr = arg1;
	int dst_sz = arg2;
	if(dst_sz < 0)
		return(-EINVAL);
	if(!vxmem_checkperm(proc->mem, dst_addr, dst_sz, VXPERM_WRITE, NULL))
		return(-EINVAL);
	
	char *dst = (char*)m->base + dst_addr;
	
	int fd = open("/dev/urandom", O_RDONLY);
	if(fd >= 0) {
		read(fd, dst, dst_sz);
		close(fd);
	}
	return( dst_sz );
}


static int (*syscalls[])(CONN *conn, struct process *process, vxproc*, vxmmap*, int*) = {
	[VXSYSEXIT]		syscall_exit,
	[VXSYSBRK]		syscall_brk,
	[VXSYSWRITE]		syscall_write,

	[VXSYSREGISTER]		syscall_register,
	[VXSYSUNREGISTER]	syscall_unregister,
	[VXSYSREADREQUESTS]	syscall_readrequests,
	[VXSYSWRITERESPONSES]	syscall_writeresponses,
	[VXSYSGET]		syscall_st_get,
	[VXSYSPREFETCH]		syscall_st_prefetch,
	[VXSYSSET]		syscall_st_set,
	[VXSYSDEL]		syscall_st_del,
	[VXSYSGETRANDOM]	syscall_st_getrandom
};

static int do_syscall(CONN *conn, struct process *process) {
	vxproc *proc = process->p;
	int ret = 0;
	int syscall_ret = SYSCALLRET_CONTINUE;
	uint32_t num =  proc->cpu->reg[EAX];
/*
	uint32_t arg1 = proc->cpu->reg[EDX];
	uint32_t arg2 = proc->cpu->reg[ECX];
	uint32_t arg3 = proc->cpu->reg[EBX];
	uint32_t arg4 = proc->cpu->reg[EDI];
	uint32_t arg5 = proc->cpu->reg[ESI];
*/
	vxmmap *m = vxmem_map(proc->mem, 0);
		
	ret = -EINVAL;
	if(num < NELEM(syscalls) && num >= 0 && syscalls[num]) {
		ret = syscalls[num](conn, process, proc, m, &syscall_ret);
	}
	proc->cpu->reg[EAX] = ret;

	//vxmem_unmap(proc->mem, m);	 XXX get rid of ref count?
	return(syscall_ret);
}


int process_load(struct process *process, char *elf_filename) {
	vxproc *volatile p = vxproc_alloc();
	log_warn("#%p started: %s  (owner=%s:%i)", process, process->encoded_key, process->owner_host, process->owner_port);
	if(NEVER(p == NULL)) {
		log_perror("#%p vxproc_alloc", process);
		return(-1);
	}
	p->allowfp = 1;

	const char *argv[]={elf_filename, NULL};
	const char *environ[]={NULL, NULL};

	int r = vxproc_loadelffile(p, elf_filename, argv, environ);
	r = r;
	if(NEVER(r < 0)) {
		log_perror("#%p vxproc_loadelffile", process);
		vxproc_free(p);
		return(-1);
	}
	process->p = p;
	return(0);
}
/*
	0 - yield
	>0 - exited nicely
	-1 - error
*/
int process_run(CONN *conn, struct process *process) {
	// Simple execution loop.
	int irq_counter = 0;
	int syscall_counter = 0;
	int syscall_limit = conn ? conn->server->syscall_limit * conn->requests +128 : INT_MAX;
	for (;;) {
	loop_again:;
		int rc = vxproc_run(process->p);
		switch(rc) {
		case VXTRAP_SYSCALL: {
			/* limit: four syscalls per request */
			if(syscall_counter++ > syscall_limit) {
				log_warn("#%p killed: too many syscalls", process);
				return(-1);
			}
			switch(do_syscall(conn, process)) {
			case SYSCALLRET_CONTINUE:
				goto loop_again;
			case SYSCALLRET_YIELD:
				if(conn && conn->server->trace)
					log_warn("#%p do yield", process);
				return(0);
			case SYSCALLRET_KILL:
				log_warn("#%p killed", process);
				return(-1);
			case SYSCALLRET_EXIT: {
				int exit_code = process->exit_code;
				log_warn("#%p exited with code %i", process, exit_code);
				exit_code = abs(exit_code);
				if(exit_code <= 0)
					exit_code = 1;
				return(exit_code); }
			default:
				log_error("assert(0)");
				return(-1);
			} }
		case VXTRAP_IRQ: {
			irq_counter++;
			if(irq_counter > 1) {
				log_warn("#%p killed due to too high cpu usage", process);
				return(-1);
			}
			goto loop_again; }
		case VXTRAP_PAGEFAULT:
			log_warn("#%p segmentation fault (Reference to inaccessible page)", process);
			return(-1);
		default:
			if (rc < 0) {
				log_perror("#%p vxproc_run", process);
			} else {
				log_warn("#%p vxproc_run trap %#x", process, rc);
			}
			return(-1);
		}
	}
}
