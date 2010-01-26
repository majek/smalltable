#include "shared.h"
#include "proxy.h"

void process_single(struct config *config, char *req_buf, int request_sz) {
	char *res_buf;
	int res_buf_sz;
	buf_get_writer(&config->res_buf, &res_buf, &res_buf_sz, MAX_REQUEST_SIZE);
	
	int produced = error_from_reqbuf(req_buf, request_sz,
						res_buf, res_buf_sz,
						MEMCACHE_STATUS_INTERNAL_ERROR);
	
	buf_produce(&config->res_buf, produced);
	return;
}

