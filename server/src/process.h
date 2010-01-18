#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <setjmp.h>

#include "libvx32/vx32.h"
#include "libvx32/args.h"

#define syscall xxxsyscall // don't redefine 'syscall' function. FIXME
#include "libvxc/syscall.h"

struct process {
	vxproc *volatile p;
	int exit_code;
	
	char *key;
	int key_sz;
	char *encoded_key;
	
	char *owner_host;
	int owner_port;
	time_t owner_time;
	
	int registercnt;

	char *user_buf;
	int user_buf_sz;
	
	struct buffer *send_buf;
	int send_sz;
};

/* process.c */
struct process *process_new(CONN *conn, char *key, int key_sz);
void process_free(struct process *process);
int process_load(struct process *process, char *elf_filename);
int process_run(CONN *conn, struct process *process);


