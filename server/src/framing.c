#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "shared.h"

int unpack_request(ST_REQ *req, char *buf, int buf_sz) {
	struct memcache_header *header = (struct memcache_header *)buf;
	req->opcode = header->opcode; // 8bit
	req->status = ntohs(header->status); // 16bit
	req->opaque = ntohl(header->opaque); // 32bit
	req->cas = ntohll(header->cas);	// 64 bit
	
	u_int32_t extras_length = header->extras_length; // 8bit
	u_int32_t key_length = ntohs(header->key_length); // 16 bit
	u_int32_t body_length = ntohl(header->body_length); // 32bit

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

/*
	-1 - broken
	 0 - need more data
	>0 - proper request
*/
int reqbuf_get_sane_request_sz(char *buf, int buf_sz) {
	if(unlikely(buf_sz < MEMCACHE_HEADER_SIZE))
		return(0);
	int request_sz = MC_GET_BODY_LENGTH(buf) + MEMCACHE_HEADER_SIZE;
	if(likely(buf_sz < request_sz))
		return(0);
	/* received enough data. now sanity. */
	int magic = MC_GET_MAGIC(buf);
	if(unlikely(0x80 != magic)) {
		log_error("incorrect magic 0x%02x", magic);
		return(-1);
	}
	uint32_t extras_length = MC_GET_EXTRAS_LENGTH(buf);
	uint32_t key_length = MC_GET_KEY_LENGTH(buf);
	uint32_t body_length = MC_GET_BODY_LENGTH(buf);
	if(unlikely(body_length < (extras_length + key_length)) ) { // value_sz < 0? (unsigned:P)
		log_error("incorrect body_length 0x%x < 0x%x + 0x%x", body_length, extras_length, key_length);
		return(-1);
	}
	u_int32_t value_length	= body_length - extras_length - key_length;
	if(unlikely(value_length > MAX_VALUE_SIZE)) {
		log_error("value_length is too big 0x%x > 0x%x", value_length, MAX_VALUE_SIZE);
		return(-1);
	}
	if(unlikely(key_length > MAX_KEY_SIZE)) {
		log_error("key_length is too big 0x%x > 0x%x", key_length, MAX_KEY_SIZE);
		return(-1);
	}
	return(request_sz);
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

void prepare_res(ST_RES *res, char *res_buf, int res_buf_sz, char *request, int request_sz) {
	ST_REQ req;
	unpack_request(&req, request, request_sz);
	
	memset(res, 0, sizeof(ST_RES));
	res->buf = res_buf + MEMCACHE_HEADER_SIZE;
	res->buf_sz = res_buf_sz - MEMCACHE_HEADER_SIZE;
	res->opcode = req.opcode;
	res->opaque = req.opaque;
}

int error_from_reqbuf(char *request, int request_sz, char *res_buf, int res_buf_sz, int status) {
	ST_RES res;
	prepare_res(&res, res_buf, res_buf_sz, request, request_sz);
	set_error_code(&res, status);
	return(pack_response(res_buf, res_buf_sz, &res));
}


static char *error_codes[] = {
	[MEMCACHE_STATUS_KEY_NOT_FOUND]		"Key not found",
	[MEMCACHE_STATUS_KEY_EXISTS]		"Key exists",
	[MEMCACHE_STATUS_VALUE_TOO_BIG]		"Value too big",
	[MEMCACHE_STATUS_INVALID_ARGUMENTS]	"Invalid arguments",
	[MEMCACHE_STATUS_ITEM_NOT_STORED]	"Item not stored",
	[MEMCACHE_STATUS_UNKNOWN_COMMAND]	"Unknown command"
};

ST_RES *set_error_code(ST_RES *res, unsigned char status) {
	res->extras_sz = 0;
	res->key_sz = 0;
	res->cas = 0;

	char *error_str = "Unknown error code";
	if(status >= 0 && status< NELEM(error_codes))
		error_str = error_codes[status];
	
	res->status = status;
	res->value = res->buf;
	res->value_sz = MIN(strlen(error_str), res->buf_sz);
	memcpy(res->value, error_str, res->value_sz);
	return(res);
}
