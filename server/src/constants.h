/* Quiet commands are implemented differently. Instead of creating Quiet
variants for every command - which is stupid - I've used 'reserved' field
to pass a QUIET flag. */
#define MEMCACHE_CMD_GET	0x00 //   Get
#define MEMCACHE_CMD_SET	0x01 //   Set
#define MEMCACHE_CMD_ADD	0x02 //   Add
#define MEMCACHE_CMD_REPLACE	0x03 //   Replace
#define MEMCACHE_CMD_DELETE	0x04 //   Delete
#define MEMCACHE_CMD_INCR	0x05 //   Increment
#define MEMCACHE_CMD_DECR	0x06 //   Decrement
#define MEMCACHE_CMD_QUIT	0x07 //   Quit
#define MEMCACHE_CMD_FLUSH	0x08 //   Flush
//#define MEMCACHE_CMD_GETQ	0x09 //   GetQ
#define MEMCACHE_CMD_NOOP	0x0A //   No-op
#define MEMCACHE_CMD_VERSION	0x0B //   Version
#define MEMCACHE_CMD_GETK	0x0C //   GetK
//#define MEMCACHE_CMD_GETKQ	0x0D //   GetKQ
#define MEMCACHE_CMD_APPEND	0x0E //   Append
#define MEMCACHE_CMD_PREPEND	0x0F //   Prepend
#define MEMCACHE_CMD_STAT	0x10 //   Stat

#define MEMCACHE_XCMD_CODE_LOAD		0x70 //   Load code
#define MEMCACHE_XCMD_CODE_UNLOAD	0x71 //   Unload code
#define MEMCACHE_XCMD_CODE_CHECK	0x72 //   Check code version



#define MEMCACHE_STATUS_OK			0x0000 // No error
#define MEMCACHE_STATUS_KEY_NOT_FOUND		0x0001 // Key not found
#define MEMCACHE_STATUS_KEY_EXISTS		0x0002 // Key exists
#define MEMCACHE_STATUS_VALUE_TOO_BIG		0x0003 // Value too big
#define MEMCACHE_STATUS_INVALID_ARGUMENTS	0x0004 // Invalid arguments
#define MEMCACHE_STATUS_ITEM_NOT_STORED		0x0005 // Item not stored
#define MEMCACHE_STATUS_UNKNOWN_COMMAND		0x0081 // Unknown command
#define MEMCACHE_STATUS_INTERNAL_ERROR		0x0082

/* QUIET flag shall be in Reserved field, that's better than having two variants
for every command */
enum{
	MEMCACHE_RESERVED_FLAG_QUIET		= 1 << 0,
	MEMCACHE_RESERVED_FLAG_PROXY_COMMAND	= 1 << 1
};
