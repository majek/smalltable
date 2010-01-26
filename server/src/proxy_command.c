#include <string.h>
#include <assert.h>

#include "proxy.h"

/* Enough data is collected. Do the magic - dispatch commands.
   Pop requests from req_buf, write replies to send_buf. 
   Return: consumed bytes. */
int process_multi(CONN *conn, char *start_req_buf, int start_req_buf_sz) {
	char *req_buf = start_req_buf;
	int req_buf_sz = start_req_buf_sz;
	char *end_req_buf = req_buf + req_buf_sz;
	
	struct config *config = (struct config*)conn->server->userdata;
	
	struct st_server *servers[MAX_SERVERS];
	int servers_no = 0;
	struct st_server *order[MAX_QUIET_REQUESTS];
	int requests = 0;
	while(end_req_buf - req_buf) {
		int request_sz = MC_GET_REQUEST_SZ(req_buf);
		
		//if(NEVER(conn->server->trace))
		//	log_info("%s:%i             cmd=0x%02x(%i)", conn->host, conn->port, cmd, cmd);
		if(MC_GET_RESERVED(req_buf) & MEMCACHE_RESERVED_FLAG_PROXY_COMMAND
				|| MC_GET_OPCODE(req_buf) == MEMCACHE_CMD_NOOP) {
			order[requests] = NULL;
			process_single(config, req_buf, request_sz);
		} else {
			char *key;
			int key_sz;
			REQBUF_GET_KEY(req_buf, &key, &key_sz);
			struct st_server *srv = find_st_server(config,
								key, key_sz);
			char *dst;
			int dst_sz;
			buf_get_writer(&srv->send_buf, &dst, &dst_sz, request_sz);
			memcpy(dst, req_buf, request_sz);
			buf_produce(&srv->send_buf, request_sz);
			
			if(0 == srv->queued_requests)
				servers[servers_no++] = srv;
			srv->queued_requests++;
			order[requests] = srv;
		}
		req_buf += request_sz;
		requests++;
	}
	assert(requests <= MAX_QUIET_REQUESTS);
	
	process_memcache(servers, servers_no);
	
	char *src, *dst;
	int src_sz, dst_sz;
	int i;
	for(i=0; i < requests; i++) {
		struct buffer *src_buf;
		if(!order[i]) {
			src_buf = &config->res_buf;
		} else {
			struct st_server *srv = order[i];
			src_buf = &srv->recv_buf;
		}
		buf_get_reader(src_buf, &src, &src_sz);
		int response_sz = MC_GET_REQUEST_SZ(src);
		buf_get_writer(&conn->send_buf, &dst, &dst_sz, response_sz);
		
		memcpy(dst, src, response_sz);
		
		buf_consume(src_buf, response_sz);
		buf_produce(&conn->send_buf, response_sz);
	}
	
	return(req_buf - start_req_buf);
}
