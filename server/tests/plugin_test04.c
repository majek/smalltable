#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

#define CMD_INCR 0x91

extern uint64_t unique_number;

struct mc_extras_incr{
	uint64_t amount;
	uint64_t initial;
	uint32_t expiration;
};

/*
   Request:
      MUST have extras.
      MUST have key.
      MUST NOT have value.
   Ignore CAS?
*/
ST_RES *cmd_incr(ST_REQ *req, ST_RES *res) {
	if(req->extras_sz != 20 || !req->key_sz || req->value_sz)
		return(set_error_code(res, MC_STATUS_INVALID_ARGUMENTS));
	struct mc_extras_incr *got_md = (struct mc_extras_incr *)req->extras;
	uint64_t amount = ntohll(got_md->amount);
	uint64_t initial = ntohll(got_md->initial);
	uint32_t expiration = ntohl(got_md->expiration);
	
	MC_METADATA md;
	res->value = res->buf;
	uint64_t *v = (uint64_t*)(res->value);

	int ret = storage_get(&md, res->value, res->buf_sz, req->key, req->key_sz);
	if(ret < 0) {
		/* If the counter does not exist, one of two things may happen:
		   1.  If the expiration value is all one-bits (0xffffffff), the
		       operation will fail with NOT_FOUND.
		   2.  For all other expiration values, the operation will succeed by
		       seeding the value for this key with the provided initial value to
		       expire with the provided expiration time.  The flags will be set
		       to zero. */
		if(expiration == 0xffffffff)
			return(set_error_code(res, MC_STATUS_KEY_NOT_FOUND));
		ret = 8;
		*v = initial;
		md.flags = 0;
		md.expires = expiration;
		md.cas = unique_number++ || unique_number++;
	} else {
		if(req->opcode == CMD_INCR) {
			*v += amount;
		} else { //DECR
			*v -= amount;
		}
		md.cas++;
	}
	if(ret != 8)
		return(set_error_code(res, MEMCACHE_STATUS_ITEM_NON_NUMERIC));
	
	int r = storage_set(&md, res->value, ret, req->key, req->key_sz);
	if(r < 0)
		return(set_error_code(res, MC_STATUS_ITEM_NOT_STORED));
	
	res->value_sz = ret;
	res->status = MC_STATUS_OK;
	res->cas = md.cas;
	return(res);
}

struct commands_pointers commands_pointers[] = {
	[MEMCACHE_CMD_INCR]	{&cmd_incr, CMD_FLAG_PREFETCH},
	[MEMCACHE_CMD_DECR]	{&cmd_incr, CMD_FLAG_PREFETCH}
};

int main(int argc, char **argv) {
	setvbuf(stdout, (char *) NULL, _IONBF, 0);

	simple_commands_loop(commands_pointers, NELEM(commands_pointers));

	return(0);
}



