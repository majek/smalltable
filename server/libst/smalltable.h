#ifndef _SMALLTABLE_H
#define _SMALLTABLE_H

#include <stdint.h>


#define SWAP(a,b)		\
	do{			\
		__typeof (a) c;	\
		c = (a);	\
		(a) = (b);	\
		(b) = (c);	\
	}while(0)

#define MIN(a,b)		\
	({ typeof (a) _a = (a);	\
	   typeof (b) _b = (b);	\
	   _a <= _b ? _a : _b; })

#define MAX(a,b)		\
	({ typeof (a) _a = (a);	\
	   typeof (b) _b = (b);	\
	   _a >= _b ? _a : _b; })

#define NELEM(x) (sizeof(x)/sizeof((x)[0]))


#if defined(__GNUC__) && 0
# define likely(X)    __builtin_expect((X),1)
# define unlikely(X)  __builtin_expect((X),0)
#else
# define likely(X)    !!(X)
# define unlikely(X)  !!(X)
#endif


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

enum {
	CMD_FLAG_QUIET		= 1 << 1,	// don't answer immedietally
	CMD_FLAG_PREFETCH	= 1 << 2	// automatically prefetch given key
};

void __exit(int status);

int st_register(int cmd, int cmd_flags);
int st_unregister(int cmd);
int st_read_requests(char *buf, int buf_sz);
int st_write_responses(char *buf, int buf_sz);

#define __bswap_constant_16(x) \
     ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#define __bswap_constant_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |               \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
# define __bswap_constant_64(x) \
     ((((x) & 0xff00000000000000ull) >> 56)                                   \
      | (((x) & 0x00ff000000000000ull) >> 40)                                 \
      | (((x) & 0x0000ff0000000000ull) >> 24)                                 \
      | (((x) & 0x000000ff00000000ull) >> 8)                                  \
      | (((x) & 0x00000000ff000000ull) << 8)                                  \
      | (((x) & 0x0000000000ff0000ull) << 24)                                 \
      | (((x) & 0x000000000000ff00ull) << 40)                                 \
      | (((x) & 0x00000000000000ffull) << 56))


//#  if __BYTE_ORDER == __LITTLE_ENDIAN
#   define ntohl(x)     __bswap_constant_32 (x)
#   define ntohs(x)     __bswap_constant_16 (x)
#   define ntohll(x)    __bswap_constant_64 (x)
#   define htonl(x)     __bswap_constant_32 (x)
#   define htons(x)     __bswap_constant_16 (x)
#   define htonll(x)    __bswap_constant_64 (x)
//# endif




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
#define MC_GET_BODY_LENGTH(buf)		ntohl(((struct memcache_header *)buf)->body_length)
#define MC_GET_OPCODE(buf)		     (((struct memcache_header *)buf)->opcode)
#define MC_GET_REQUEST_SZ(buf)	\
	(MEMCACHE_HEADER_SIZE + MC_GET_BODY_LENGTH(buf))


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
int reqbuf_get_request_sz(char *buf, int buf_sz);
ST_RES *set_error_code(ST_RES *res, int status);



#define MEMCACHE_STATUS_OK			0x0000 // No error
#define MEMCACHE_STATUS_KEY_NOT_FOUND		0x0001 // Key not found
#define MEMCACHE_STATUS_KEY_EXISTS		0x0002 // Key exists
#define MEMCACHE_STATUS_VALUE_TOO_BIG		0x0003 // Value too big
#define MEMCACHE_STATUS_INVALID_ARGUMENTS	0x0004 // Invalid arguments
#define MEMCACHE_STATUS_ITEM_NOT_STORED		0x0005 // Item not stored
#define MEMCACHE_STATUS_ITEM_NON_NUMERIC	0x0006 // Incr/Decr on non-numeric value.
#define MEMCACHE_STATUS_UNKNOWN_COMMAND		0x0081 // Unknown command
#define MEMCACHE_STATUS_INTERNAL_ERROR		0x0082


struct mc_metadata{
	uint32_t flags;
	uint32_t expiration;
	uint64_t cas;
};
typedef struct mc_metadata MC_METADATA;

#define READ_REQUESTS_BUF_SZ MAX_REQUEST_SIZE
#define WRITE_RESPONSES_BUF_SZ (MAX_REQUEST_SIZE + sizeof(MC_METADATA))

#define MEMCACHE_CMD_GET	0x00 //   Get
#define MEMCACHE_CMD_SET	0x01 //   Set
#define MEMCACHE_CMD_ADD	0x02 //   Add
#define MEMCACHE_CMD_REPLACE	0x03 //   Replace
#define MEMCACHE_CMD_DELETE	0x04 //   Delete
#define MEMCACHE_CMD_INCR	0x05 //   Increment
#define MEMCACHE_CMD_DECR	0x06 //   Decrement
#define MEMCACHE_CMD_QUIT	0x07 //   Quit
#define MEMCACHE_CMD_FLUSH	0x08 //   Flush
#define MEMCACHE_CMD_GETQ	0x09 //   GetQ
#define MEMCACHE_CMD_NOOP	0x0A //   No-op
#define MEMCACHE_CMD_VERSION	0x0B //   Version
#define MEMCACHE_CMD_GETK	0x0C //   GetK
#define MEMCACHE_CMD_GETKQ	0x0D //   GetKQ
#define MEMCACHE_CMD_APPEND	0x0E //   Append
#define MEMCACHE_CMD_PREPEND	0x0F //   Prepend
#define MEMCACHE_CMD_STAT	0x10 //   Stat



void fatal(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

struct commands_pointers {
	ST_RES *(*fun)(ST_REQ *req, ST_RES *res);
	int flags;
};

void simple_commands_loop(struct commands_pointers *cmd_ptr, int cmd_ptr_sz);

int storage_get(MC_METADATA *md, char *value, int value_sz, char *key, int key_sz);
int storage_set(MC_METADATA *md, char *value, int value_sz, char *key, int key_sz);
int storage_delete(char *key, int key_sz);
void storage_prefetch(char **keys, int *key_sz, int items_counter);



#endif // _SMALLTABLE_H
