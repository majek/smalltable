#include <string.h>

#include "shared.h"
#include "proxy.h"

static ST_RES *cmd_unknown(CONN *conn, ST_REQ *req, ST_RES *res);
static ST_RES *cmd_get_config(CONN *conn, ST_REQ *req, ST_RES *res);
static ST_RES *cmd_set_config(CONN *conn, ST_REQ *req, ST_RES *res);
static ST_RES *cmd_stop(CONN *conn, ST_REQ *req, ST_RES *res);
static ST_RES *cmd_start(CONN *conn, ST_REQ *req, ST_RES *res);
static ST_RES *cmd_noop(CONN *conn, ST_REQ *req, ST_RES *res);


typedef ST_RES* (*system_cmd_t)(CONN *conn, ST_REQ *req, ST_RES *res);

struct {
	system_cmd_t foo;
} cmd_pointers[] = {
	[PROXY_CMD_GET_CONFIG]		= {&cmd_get_config},
	[PROXY_CMD_SET_CONFIG]		= {&cmd_set_config},
	[PROXY_CMD_STOP]		= {&cmd_stop},
	[PROXY_CMD_START]		= {&cmd_start},
	[MEMCACHE_CMD_NOOP]		= {&cmd_noop}
};


void process_single(CONN *conn, char *req_buf, int request_sz) {
	struct config *config = (struct config*)conn->server->userdata;
	
	char *res_buf;
	int res_buf_sz;
	buf_get_writer(&config->res_buf, &res_buf, &res_buf_sz, MAX_REQUEST_SIZE);
	
	ST_REQ req;
	int r = unpack_request(&req, req_buf, request_sz);

	ST_RES res;
	memset(&res, 0, sizeof(res));
	res.opcode = req.opcode;
	res.opaque = req.opaque;
	res.buf = res_buf + MEMCACHE_HEADER_SIZE;
	res.buf_sz = res_buf_sz - MEMCACHE_HEADER_SIZE;
	if(r < 0) {
		set_error_code(&res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL);
		goto exit;
	}

	
	int cmd = req.opcode;
	system_cmd_t foo = cmd_unknown;
	if(cmd >= 0 && cmd < NELEM(cmd_pointers)) {
		foo = cmd_pointers[cmd].foo;
	}
	
	foo(conn, &req, &res);
	
exit:;
	int produced = pack_response(res_buf, res_buf_sz, &res);
	buf_produce(&config->res_buf, produced);
	return;
}

static ST_RES *cmd_unknown(CONN *conn, ST_REQ *req, ST_RES *res) {
	return(set_error_code(res, MEMCACHE_STATUS_UNKNOWN_COMMAND, NULL));
}

static ST_RES *cmd_get_config(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));

	struct config *config = (struct config*)conn->server->userdata;
	res->value = res->buf;
	int r = config_to_string(config, res->value, res->buf_sz);
	if(r < 1)
		return(set_error_code(res, MEMCACHE_STATUS_VALUE_TOO_BIG, NULL));
	res->value_sz = r;
	return(res);
}

static ST_RES *cmd_set_config(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || !req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
		
	struct config *config = (struct config*)conn->server->userdata;
	char *buf = (char*)malloc(req->value_sz+1);
	memcpy(buf, req->value, req->value_sz);
	buf[req->value_sz] = '\0'; // yes, do strip the last character
	
	/* pretend to load, it modifies stuff inplace */
	char *err = load_config_from_string(config, buf, 1);
	if(err) {
		res->value = res->buf;
		strncpy(res->value, err, res->buf_sz);
		res->value_sz = strlen(res->value);
		res->status = MEMCACHE_STATUS_ITEM_NOT_STORED;
		free(buf);
		return(res);
	}
	
	if(!conn->server->stopped) {
		free(buf);
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND, "not stopped"));
	}

	memcpy(buf, req->value, req->value_sz);
	buf[req->value_sz] = '\0'; // yes, do strip the last character
	
	/* really load */
	flush_config(config);
	err = load_config_from_string(config, buf, 0);
	if(err) {
		log_error("Trying to load broken config. That's really bad.");
		fatal("Lost previous config. Don't know what to do.");
	}
	
	if(0 != save_config(config)) {
		log_error("Failed to save config.");
	}
	log_error("Saved config.");
	
	res->status = MEMCACHE_STATUS_OK;
	free(buf);
	return(res);
}

static ST_RES *cmd_stop(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
	int i = conn_stop(conn->server, conn);
	log_info("%s:%i suspended %i connections", conn->host, conn->port, i);
	res->status = MEMCACHE_STATUS_OK;
	return(res);
}

static ST_RES *cmd_start(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
	if(!conn->server->stopped)
		return(set_error_code(res, MEMCACHE_STATUS_KEY_NOT_FOUND, "not stopped"));
	int i = conn_start(conn->server);
	log_info("%s:%i restored %i connections",conn->host, conn->port, i-1); // including myself, so subtract one
	res->status = MEMCACHE_STATUS_OK;
	return(res);
}

static ST_RES *cmd_noop(CONN *conn, ST_REQ *req, ST_RES *res) {
	if(req->extras_sz || req->key_sz || req->value_sz)
		return(set_error_code(res, MEMCACHE_STATUS_INVALID_ARGUMENTS, NULL));
	res->status = MEMCACHE_STATUS_OK;
	return(res);
}
