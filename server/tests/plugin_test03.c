#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

#define MY_CMD 0x91


int process_bytestream(char *request, int request_sz, char *res_buf, int res_buf_sz) {
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
	
	res.status = MEMCACHE_STATUS_OK;
	res.value = res.buf;
	res.value_sz = snprintf(res.value, res.buf_sz, "kalesony!");
	
exit:;
	return( pack_response(res_buf, res_buf_sz, &res) );
}

int main(int argc, char **argv) {
	setvbuf(stdout, (char *) NULL, _IONBF, 0);

	static char req_buf[READ_REQUESTS_BUF_SZ];	// not on stack
	static char res_buf[WRITE_RESPONSES_BUF_SZ];	// not on stack
	st_register(MY_CMD, 0);
	while(1) {
		char *buf = req_buf;
		int buf_sz = st_read_requests(req_buf, sizeof(req_buf));
		if(buf_sz < 0)
			break;
		char *buf_end = buf + buf_sz;
		while(buf_end-buf) {
			int request_sz = MC_GET_REQUEST_SZ(buf);
			int res_sz = process_bytestream(buf, request_sz,
							res_buf, sizeof(res_buf));
			
			st_write_responses(res_buf, res_sz);
			buf += request_sz;
		}
	}	
	return(0);
}

