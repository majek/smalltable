#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "shared.h"
#include "st.h"

typedef ST_RES* (*system_cmd_t)(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);
typedef ST_RES* (*system_extra_cmd_t)(CONN *conn, ST_REQ *req, ST_RES *res);


static int process_single(CONN *conn, char *request, int request_sz,
				char *res_buf, int res_buf_sz,
				int cmd_flags, void *cmd_ptr) {
	ST_REQ req;
	int r = unpack_request(&req, request, request_sz);

	ST_RES res;
	memset(&res, 0, sizeof(res));
	res.opcode = req.opcode;
	res.opaque = req.opaque;
	res.buf = res_buf + MEMCACHE_HEADER_SIZE;
	res.buf_sz = res_buf_sz - MEMCACHE_HEADER_SIZE;
	if(r < 0) {
		set_error_code(&res, MEMCACHE_STATUS_INVALID_ARGUMENTS);
		goto exit;
	}

	if(unlikely(NULL == cmd_ptr)) {
		cmd_ptr = (void*)&cmd_unknown;
		cmd_flags = 0;
	}
	
	if(unlikely(cmd_flags & CMD_FLAG_EXTRA_PARAM)) {
		// extended system command, connection given instead of API
		system_extra_cmd_t fun = (system_extra_cmd_t)cmd_ptr;
		fun(conn, &req, &res);
	} else {
		// normal system command - API
		system_cmd_t fun = (system_cmd_t)cmd_ptr;
		fun(CONFIG(conn)->api, &req, &res);
	}
exit:;
	return( pack_response(res_buf, res_buf_sz, &res) );
}

void builtin_commands_callback(CONN *conn, char *req_buf, int req_buf_sz,
				struct buffer *send_buf,
				int cmd_flags, void *cmd_ptr) {
	
	char *end_req_buf = req_buf + req_buf_sz;
	while(end_req_buf-req_buf) {
		int request_sz = MC_GET_REQUEST_SZ(req_buf);
		if(0 == request_sz)
			break;
		char *res_buf;
		int res_buf_sz;
		buf_get_writer(send_buf, &res_buf, &res_buf_sz, MAX_REQUEST_SIZE + sizeof(MC_METADATA));
		int produced = 0;
		if(buf_get_used(&conn->send_buf) < MAX_REQUEST_SIZE*3) {
			// enough place in send buffer
			produced = process_single(conn,
						req_buf, request_sz,
						res_buf, res_buf_sz,
						cmd_flags, cmd_ptr);
		} else {
			produced = error_from_reqbuf(req_buf, request_sz,
						res_buf, res_buf_sz,
						MEMCACHE_STATUS_INTERNAL_ERROR);
			// not enough
		}
		buf_produce(send_buf, produced);
		req_buf += request_sz;
	}
}

ST_RES *cmd_unknown(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res) {
	return(set_error_code(res, MEMCACHE_STATUS_UNKNOWN_COMMAND));
}


/*
Get
   Request:
	MUST NOT have extras.
	MUST have key.
	MUST NOT have value.

   Response (if found):
	MUST have extras (flags).
	MAY have key.
	MAY have value.
*/
ST_RES *cmd_get(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || !req->key_sz || req->value_sz)	
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS));
	
	MC_METADATA md;
	
	res->extras = res->buf;
	res->value  = res->buf + sizeof(md.flags);

	int ret = storage_get(api, &md, res->value, res->buf_sz - sizeof(md.flags), req->key, req->key_sz); //overcommiting
	if(ret < 0)
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND));

	u_int32_t flags = htonl(md.flags);
	memcpy(res->extras, &flags, sizeof(flags));
	res->extras_sz = sizeof(flags);
	res->status = MEMCACHE_STATUS_OK;
	res->value_sz = ret;
	res->cas = md.cas;
	return(res);
}

/*
Set
  Request:
	MUST have extras. (flags, expiration)
	MUST have key.
	MUST have value. (for me empty value is okay)
  Response:
	nothing
*/
ST_RES *cmd_set(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz != 8 || !req->key_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS));

	MC_METADATA *got_md = (MC_METADATA*)req->extras;
	MC_METADATA md;
	md.flags = ntohl(got_md->flags);
	md.expiration = ntohl(got_md->expiration);
	md.cas = ++unique_number || ++unique_number;
	
	MC_METADATA curr_md;
	int ret = 0;
	if(req->cas || req->opcode != MEMCACHE_CMD_SET) {
		/* we're interested only in curr_md, buf we still need to give proper buffer */
		ret = storage_get(api, &curr_md, res->buf, res->buf_sz, req->key, req->key_sz);
	}
	if(req->cas) { // the requested operation MUST only succeed if the item exists and has a CAS value identical to the provided value.
		if(ret < 0)
			return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND));
		if(req->cas && curr_md.cas != req->cas)
			return(set_error_code(res, MEMCACHE_STATUS_ITEM_NOT_STORED));
	}
	if(req->opcode == MEMCACHE_CMD_ADD && ret > -1) // Add MUST fail if the item already exist.
		return(set_error_code(res, MEMCACHE_STATUS_KEY_EXISTS));
	
	if(req->opcode == MEMCACHE_CMD_REPLACE && ret < 0) // Replace MUST fail if the item doesn't exist.
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND));
	
	ret = storage_set(api, &md, req->value, req->value_sz, req->key, req->key_sz);
	if(NEVER(ret < 0))
		return(set_error_code(res, MEMCACHE_STATUS_ITEM_NOT_STORED));
	
	res->cas = md.cas;
	res->status = MEMCACHE_STATUS_OK;
	return(res);
}
/*
   Request:
	MUST NOT have extras
	MUST have key.
	MUST NOT have value.
   Ignore CAS?
*/
ST_RES *cmd_delete(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || !req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS));

	int ret = storage_delete(api, req->key, req->key_sz);
	if(ret < 0)
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND));

	res->status = MEMCACHE_STATUS_OK;
	return(res);
}

/*
   Request:
      MUST NOT have extras.
      MUST NOT have key.
      MUST NOT have value.
*/
ST_RES *cmd_noop(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS));

	res->status = MEMCACHE_STATUS_OK;
	return(res);
}

/*
   Request:
      MUST NOT have extras.
      MUST NOT have key.
      MUST NOT have value.
*/
ST_RES *cmd_version(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS));

	res->value = res->buf;
	res->value_sz = snprintf(res->value, res->buf_sz, "%s", VERSION_STRING);
	res->status = MEMCACHE_STATUS_OK;
	return(res);
}
