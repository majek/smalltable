#ifndef _BUFFER_H
#define _BUFFER_H

#include <stdlib.h>
#include "coverage.h"

/*
 buf_sz:                             v
 buf:        xxxxxxxxxXXXXXxxxxxxxxxx
 start_off:           ^
 stop_off:                ^
*/
struct buffer {
	char *buf;
	int buf_sz;
	int start_off;
	int stop_off;
};

void buf_get_reader(struct buffer *buf, char **ubuf_ptr, int *ubuf_sz_ptr);
void buf_get_writer(struct buffer *buf, char **ubuf_ptr, int *ubuf_sz_ptr, int min_size);
void buf_produce(struct buffer *buf, int produced_sz);
void buf_rollback_produced(struct buffer *buf, int rollback_sz);
void buf_consume(struct buffer *buf, int consumed_sz);
void buf_free(struct buffer *buf);
int buf_get_used(struct buffer *buf);


void pool_free();
void get_pool_size(int *counter_ptr, int *memory_wasted_ptr);

static inline void* st_malloc(int size) {
	void *buf = malloc( size );
	if(NEVER(NULL == buf)) {
		pool_free();
		buf = malloc( size );
		if(NULL == buf) {
			log_error("Can't allocate memory!");
			fatal("Can't allocate memory!");
		}
	}
	return(buf);
}

static inline void* st_realloc(void *user_buf, int size) {
	void *buf = realloc(user_buf, size);
	if(NEVER(NULL == buf)) {
		pool_free();
		buf = realloc(user_buf, size);
		if(NULL == buf) {
			log_error("Can't re-allocate memory!");
			fatal("Can't re-allocate memory!");
		}
	}
	return(buf);
}

static inline void* st_calloc(int items, int size) {
	void *buf = calloc(items, size);
	if(NEVER(NULL == buf)) {
		pool_free();
		buf = calloc(items, size);
		if(NULL == buf) {
			log_error("Can't re-allocate memory!");
			fatal("Can't re-allocate memory!");
		}
	}
	return(buf);
}

void pool_get(char **buf_ptr, int *buf_sz_ptr);
void pool_put(char *buf, int buf_sz);


#endif // _BUFFER_H

