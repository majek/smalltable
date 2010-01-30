#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

void fatal(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

void fatal(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fflush(stdout);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\nQUITTING!\n");
	va_end(ap);
	exit(1);
}


void global_log(const char *type, const char *fmt, ...) {
	char buf[1024];
	va_list p;
	va_start(p, fmt);
	if(fmt == 0) fmt = "";
	vsnprintf(buf, sizeof(buf), fmt, p);
	va_end(p);
	fflush(stdout);
	fprintf(stderr, " [*] %8s  %s\n", type, buf);
	fflush(stderr);
}

void global_log_perror(const char *type, const char *fmt, ...) {
	int err = errno;
	char buf[1024];
	va_list p;
	va_start(p, fmt);
	if(fmt == 0) fmt = "";
	fflush(stdout);
	vsnprintf(buf, sizeof(buf), fmt, p);
	va_end(p);
	
	char buf2[1024];
	snprintf(buf2, sizeof(buf2), "%s%s%s",
				buf,
				strlen(buf) ? " : " : "",
				strerror(err));
	global_log(type, buf2);
	return;
}

void global_error(const char *filename, int line, const char *type, const char *fmt, ...) {
	char buf[1024];
	va_list p;
	va_start(p, fmt);
	if(fmt == 0) fmt = "";
	fflush(stdout);
	vsnprintf(buf, sizeof(buf), fmt, p);
	va_end(p);
	
	char buf2[1024];
	snprintf(buf2, sizeof(buf2), "%s:%i  %s",
				filename,
				line,
				buf);
	global_log(type, buf2);
}

/*
unsigned long hash_djb2(char *s) {
	unsigned char *str=(unsigned char*)s;
	unsigned long hash = 5381;
	int c;
	while ( (c = *str++) )
		hash = ((hash << 5) + hash) + c; // hash * 33 + c 
	return hash;
}
*/

unsigned long hash_sum(char *str) {
	unsigned long hash = 0;
	unsigned int c;
	while ( (c = *str++) )
		hash += c;
	return hash;
}

unsigned long hash_kr(char *p) {
	unsigned long  hash, a = 31415, b = 27183;
	for (hash = 0; *p; p++, a = (a * b))
		hash = hash * a + *p;
	return hash;
}


char translate[] = "___________________________________#_______+,-._0123456789_;<=>_@ABCDEFGHIJKLMNOPQRSTUVWXYZ[_]^__abcdefghijklmnopqrstuvwxyz{|}~_";
char tohextrans[] = "0123456789ABCDEF";


int key_escape(char *dst, int dst_sz, char *key, int key_sz){
	int p=0, i;
	for(i=0; i<key_sz && (dst_sz-p-5)>0; i++) {
		int c = key[i];
		if(c >= sizeof(translate) || translate[c] == '_') {
			dst[p++] = '%';
			dst[p++] = tohextrans[ (c >> 4) & 0xF ];
			dst[p++] = tohextrans[ c & 0xF ];
		} else {
			dst[p++] = c;
		}
	}
	dst[p] = '\0';
	assert(p < dst_sz);
	return p;
}

static int get_fd_size(int fd, u_int64_t *size) {
	struct stat st;
	if(fstat(fd, &st) != 0){
		log_perror("stat()");
		return(-1);
	}
	if(size)
		*size = st.st_size;
	return(0);
}

char *read_full_file(char *filename) {
	int fd = open(filename, O_RDONLY);
	if(fd < 0)
		return(NULL);
	
	u_int64_t file_size;
	if(-1 == get_fd_size(fd, &file_size)) {
		goto error;
	}
	char *buf = (char*)malloc(file_size);
	int ret = read(fd, buf, file_size);
	if(ret != file_size) {
		free(buf);
		goto error;
	}
	close(fd);
	return(buf);
error:;
	close(fd);
	return(NULL);
}
