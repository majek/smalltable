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


char translate[] = "___________________________________#_______+,-__0123456789_;<=>_@ABCDEFGHIJKLMNOPQRSTUVWXYZ[_]^__abcdefghijklmnopqrstuvwxyz{|}~_";
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

const int hex_to_int[256] = { ['0'] 0,   ['1'] 1,   ['2'] 2,   ['3'] 3,
			['4'] 4,   ['5'] 5,   ['6'] 6,   ['7'] 7,
			['8'] 8,   ['9'] 9,   ['A'] 10,  ['B'] 11,
			['C'] 12,  ['D'] 13,  ['E'] 14,  ['F'] 15,
					      ['a'] 10,  ['b'] 11,
			['c'] 12,  ['d'] 13,  ['e'] 14,  ['f'] 15};

int key_unescape(char *src, char *key, int key_sz) {
	int i = 0;
	while(*src) {
		int c = *src++;
		if(c == '%') {
			int src_a = *src++;
			int src_b = *src++;
			if(src_a == '\0' || src_b == '\0')
				return -1;
			
			int a = hex_to_int[src_a];
			int b = hex_to_int[src_b];
			if((a == 0 && src_a != '0') || (b == 0 && src_b != '0')) {
				return -1;
			}
			key[i++] = (a << 4) | b;
		} else {
			key[i++] = c;
		}
		if(i == key_sz)
			return -1;
	}
	return i;
}


int is_dir(char *path) {
	struct stat st;
	if(0 != stat(path, &st)) { // may happen. if dir is not there
		return(0);
	}
	if(!S_ISDIR(st.st_mode))
		return(0);
	return(1);
}

int fd_size(int fd, u_int64_t *size) {
	struct stat st;
	if(fstat(fd, &st) != 0){ // never
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
	if(-1 == fd_size(fd, &file_size)) { // never
		goto error;
	}
	char *buf = (char*)malloc(file_size+1);
	int ret = read(fd, buf, file_size);
	if(ret != file_size) { // never
		free(buf);
		goto error;
	}
	close(fd);
	buf[file_size] = '\0';
	return(buf);
error:; { // never
	close(fd);
	return(NULL);
	}
}


char *encode_hex(char *key, int key_sz) {
	const char int_to_hex[] = "0123456789ABCDEF";
	static char hex[512 + 2];
	if(key_sz > 256) { // never
		log_error("broken key_sz %i", key_sz);
		key_sz = 256;
	}
	char *p = hex;
	while(key_sz) {
		*p++ = int_to_hex[((*key) >> 4) & 0xF];
		*p++ = int_to_hex[(*key) & 0xF] ;
		
		key++;
		key_sz--;
	}
	*p = '\0';
	return(hex);
}

int decode_hex(char **key_ptr, int *key_sz_ptr, char *hex) {
	static char start_key[256];
	char *key = start_key;
	char *end_key = key + sizeof(start_key);
	
	if(hex == NULL) {
		*key_sz_ptr = 0;
		return(0);
	}
	
	while(*hex && *(hex+1)) {
		if(key >= end_key) {
			log_error("x");
			return(-1);
		}
		int a = hex_to_int[(int)*(hex+0)];
		int b = hex_to_int[(int)*(hex+1)];
		if((a == 0 && *(hex+0) != '0') || (b == 0 && *(hex+1) != '0')) {
			log_error("x %i %c, %i %c", a, *hex, b, *(hex+1));
			return(-1);
		}
		*key++ = (a << 4) | b;
		hex += 2;
	}
	if(*hex) {
		log_error("x");
		return(-1);
	}
	*key_ptr = start_key;
	*key_sz_ptr = key - start_key;
	return(0);
}
