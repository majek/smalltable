#ifndef _FRAMING_H
#define _FRAMING_H
#include <arpa/inet.h> // ntohl
#include "coverage.h" // unlikely

#define MEMCACHE_HEADER_SIZE (24)
#define MAX_EXTRAS_SIZE (256)
#define MAX_KEY_SIZE (256)
#define MAX_VALUE_SIZE (4*1024*1024)
#define MAX_REQUEST_SIZE (MEMCACHE_HEADER_SIZE + MAX_EXTRAS_SIZE + MAX_KEY_SIZE + MAX_VALUE_SIZE + 32)

struct memcache_header{
	// network byte order
	uint8_t magic;
	uint8_t opcode;
	uint16_t key_length;
	uint8_t extras_length;
	uint8_t data_type;
	uint16_t status;
	uint32_t body_length;
	uint32_t opaque;
	uint64_t cas;
};
#define MC_GET_BODY_LENGTH(buf)		(ntohl(((struct memcache_header *)buf)->body_length))
#define MC_GET_EXTRAS_LENGTH(buf)	      (((struct memcache_header *)buf)->extras_length)
#define MC_GET_KEY_LENGTH(buf)		(ntohs(((struct memcache_header *)buf)->key_length))
#define MC_GET_MAGIC(buf)		      (((struct memcache_header *)buf)->magic)
#define MC_GET_OPCODE(buf)		      (((struct memcache_header *)buf)->opcode)

#define MC_GET_REQUEST_SZ(buf)	\
	(MEMCACHE_HEADER_SIZE + MC_GET_BODY_LENGTH(buf))

#define REQBUF_GET_KEY(buf, key_ptr, key_sz_ptr)	\
	do{						\
		*(key_sz_ptr) = MC_GET_KEY_LENGTH(buf);	\
		*(key_ptr) = (buf)+MEMCACHE_HEADER_SIZE+MC_GET_EXTRAS_LENGTH(buf);	\
	}while(0);



#define Frame_Request_Prefix	\
	uint8_t opcode;	\
	uint16_t status;	\
	uint32_t opaque;	\
	uint64_t cas;		\
	char *key;		\
	uint32_t key_sz;	\
	char *extras;		\
	uint32_t extras_sz;	\
	char *value;		\
	uint32_t value_sz;	\

struct st_req{
	Frame_Request_Prefix;
};
struct st_res{
	Frame_Request_Prefix;
	char *buf;
	int buf_sz;
};
typedef struct st_req ST_REQ;
typedef struct st_res ST_RES;

int unpack_request(ST_REQ *req, char *buf, int buf_sz);
int pack_response(char *buf, int buf_sz, ST_RES *res);

int reqbuf_get_sane_request_sz(char *buf, int buf_sz);
int error_from_reqbuf(char *request, int request_sz, char *res_buf, int res_buf_sz, int status);


#endif // _FRAMING_H
