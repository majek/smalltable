#include <stdio.h>

#include "shared.h"
#include "proxy.h"


void process_memcache(struct st_server *servers[], int servers_sz) {
	int i;
	for(i=0; i < servers_sz; i++) {
		struct st_server *srv = servers[i];
		char *req;
		int req_sz;
		buf_get_reader(&srv->send_buf, &req, &req_sz);
		char *req_end = req + req_sz;
		while(req_end - req) {
			int request_sz = MC_GET_REQUEST_SZ(req);
			
			char *res_buf;
			int res_buf_sz;
			buf_get_writer(&srv->recv_buf, &res_buf, &res_buf_sz, MAX_REQUEST_SIZE);
			int produced = error_from_reqbuf(req, request_sz,
							res_buf, res_buf_sz,
							MEMCACHE_STATUS_INTERNAL_ERROR);
			
			buf_produce(&srv->recv_buf, produced);

			
			req += request_sz;
		}
		buf_consume(&srv->send_buf, req_sz);
		
		srv->queued_requests = 0;
	
	return;
	}
}

