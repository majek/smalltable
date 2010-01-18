#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "buffer.h"

// a bit more than 4megs
#define INITIAL_BUF_SIZE ((4096+128)*1024)

struct mem_pool_item;

/* pointer to this item is also pointer to memory allocated by malloc. */
struct mem_pool_item {
	struct mem_pool_item *next;
	int buf_sz;
	char rest[];
};


static struct mem_pool_item *mem_pool_first;

static void pool_get(char **buf_ptr, int *buf_sz_ptr) {
	if(NULL == mem_pool_first) {
		*buf_ptr = (char*)st_malloc( INITIAL_BUF_SIZE );
		*buf_sz_ptr = INITIAL_BUF_SIZE;
		return;
	}
	struct mem_pool_item *item = mem_pool_first;
	mem_pool_first = item->next;
	*buf_ptr = (char*)item;
	*buf_sz_ptr = item->buf_sz;
	return;
}

static void pool_put(char *buf, int buf_sz) {
	struct mem_pool_item *item = (struct mem_pool_item *)buf;
	item->buf_sz = buf_sz;
	item->next = mem_pool_first;
	mem_pool_first = item;
	return;
}

void get_pool_size(int *counter_ptr, int *memory_wasted_ptr) {
	int counter = 0;
	unsigned long long memory_wasted = 0;
	struct mem_pool_item *item;
	for(item = mem_pool_first; item; item = item->next) {
		counter++;
		memory_wasted += item->buf_sz;
	}
	*counter_ptr = counter;
	*memory_wasted_ptr = memory_wasted;
}

void pool_free() {
	struct mem_pool_item *item = mem_pool_first;
	while( item ) {
		struct mem_pool_item *next = item->next;
		free(item);
		item = next;
	}
	mem_pool_first = NULL;
}

static inline void _buf_alloc(struct buffer *b, int min_size) {
	if(NULL == b->buf) {
		pool_get(&b->buf, &b->buf_sz);
	}
	int do_realloc = 0;
	while( (b->buf_sz-b->stop_off) <= min_size ) {
		b->buf_sz += INITIAL_BUF_SIZE;
		do_realloc = 1;
	}
	if(do_realloc)
		b->buf = (char*)st_realloc(b->buf, b->buf_sz);
	assert(b->buf);
}

static inline void _buf_free(struct buffer *b) {
	pool_put(b->buf, b->buf_sz);
	b->buf_sz = 0;
	b->buf = NULL;
}

void buf_free(struct buffer *buf) {
	if(buf->buf) {
		_buf_free(buf);
	}
}

void buf_get_writer(struct buffer *buf, char **ubuf_ptr, int *ubuf_sz_ptr, int min_size) {
	_buf_alloc(buf, min_size);
	if(ubuf_ptr)
		*ubuf_ptr = &buf->buf[ buf->stop_off ];
	if(ubuf_sz_ptr)
		*ubuf_sz_ptr = buf->buf_sz - buf->stop_off;
}

void buf_get_reader(struct buffer *buf, char **ubuf_ptr, int *ubuf_sz_ptr) {
	if(ubuf_ptr)
		*ubuf_ptr = &buf->buf[ buf->start_off ];
	if(ubuf_sz_ptr)
		*ubuf_sz_ptr = buf->stop_off - buf->start_off;
}

int buf_get_used(struct buffer *buf) {
	return(buf->stop_off - buf->start_off);
}

void buf_produce(struct buffer *buf, int produced_sz) {
	buf->stop_off += produced_sz;
}

void buf_rollback_produced(struct buffer *buf, int rollback_sz) {
	buf->stop_off -= rollback_sz;
}

void buf_consume(struct buffer *buf, int consumed_sz) {
	buf->start_off += consumed_sz;
	if(buf->start_off == buf->stop_off) {
		buf->start_off = 0;
		buf->stop_off = 0;
		_buf_free(buf);
	}
}



