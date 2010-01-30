#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "shared.h"
#include "proxy.h"

#include "uevent.h"

static int do_start(struct uevent *uevent, struct st_server *srv, struct timespec *now);
static int do_connected(struct uevent *uevent, int sd, int mask, struct st_server *srv);
static int do_write(struct uevent *uevent, int sd, int mask, struct st_server *srv);
static int do_read(struct uevent *uevent, int sd, int mask, struct st_server *srv);

static int do_done(struct uevent *uevent, struct st_server *srv);

static int do_error(struct uevent *uevent, struct st_server *srv);
static int do_connection_error(struct uevent *uevent, struct st_server *srv, struct timespec *now);


void process_memcache(CONN *conn, struct st_server *servers[], int servers_sz) {
	struct uevent uevent;
	uevent_new(&uevent);

	int i;
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for(i=0; i < servers_sz; i++) {
		do_start(&uevent, servers[i], &t0);
	}
	int r = uevent_loop(&uevent);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	log_info("%s:%i %i remote servers took %i events, %llims",
						conn->host, conn->port,
						servers_sz, r,
						TIMESPEC_MSEC_SUBTRACT(t1, t0));
/*
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
*/
}


static int do_start(struct uevent *uevent, struct st_server *srv, struct timespec *now) {
	if(srv->sd < 0) {
		if(!TIMESPEC_BEFORE(srv->next_retry, *now)) {
			return do_error(uevent, srv);
		}
		log_debug("server %s:%i connecting", srv->host, srv->port);
		int sd = net_connect(srv->host, srv->port);
		if(sd < 0) {
			return do_connection_error(uevent, srv, now);
		}
		srv->sd = sd;
		return uevent_yield(uevent, sd, UEVENT_WRITE, (uevent_callback_t)do_connected, (void*)srv);
	}
	return do_write(uevent, srv->sd, UEVENT_WRITE, srv);
}

static int do_connected(struct uevent *uevent, int sd, int mask, struct st_server *srv) {
	int optval = 0;
	socklen_t optlen;
	if(0 != getsockopt(sd, SOL_SOCKET, SO_ERROR, (char *) &optval, &optlen)) {
		goto error;
	}
	if(optval) { /* error on connect */
		log_debug("server %s:%i: %s", srv->host, srv->port, strerror(optval));
		goto error;
	}
	log_debug("optval %i %i=%i", optval, sd, srv->sd);
	return do_write(uevent, srv->sd, UEVENT_WRITE, srv);
error:;
	return do_connection_error(uevent, srv, NULL);
}

static int do_write(struct uevent *uevent, int sd, int mask, struct st_server *srv) {
	char *buf;
	int buf_sz;
	buf_get_reader(&srv->send_buf, &buf, &buf_sz);
	
	errno = 0;
	int r = send(srv->sd, buf+srv->send_offset, buf_sz-srv->send_offset, MSG_DONTWAIT);
	if(r < 1) {
		if(EAGAIN != errno) {
			log_perror("send(%s:%i)", srv->host, srv->port);
			return do_connection_error(uevent, srv, NULL);
		}
		r = 0;
	}
	srv->send_offset += r;
	if(srv->send_offset != buf_sz) {
		return uevent_yield(uevent, srv->sd, UEVENT_WRITE, (uevent_callback_t)do_write, (void*)srv);
	}
	/* done with sending */
	return uevent_yield(uevent, srv->sd, UEVENT_READ, (uevent_callback_t)do_read, (void*)srv);
}

static int do_read(struct uevent *uevent, int sd, int mask, struct st_server *srv) {
	char *buf;
	int buf_sz;
retry:;
	buf_get_writer(&srv->recv_buf, &buf, &buf_sz, 65536);
	
	errno = 0;
	int r = recv(srv->sd, buf+srv->recv_offset, buf_sz - srv->recv_offset, MSG_DONTWAIT);
	if(r < 1) {
		if(EAGAIN != errno) {
			log_perror("recv(%s:%i)", srv->host, srv->port);
			return do_connection_error(uevent, srv, NULL);
		}
		r = 0;
	}
	buf_produce(&srv->recv_buf, r);
	if(r == buf_sz - srv->recv_offset)
		goto retry;
	
swallow_next_request:;
	char *data;
	int data_sz;
	buf_get_reader(&srv->recv_buf, &data, &data_sz);
	
	char *new_data = data + srv->recv_offset;
	int new_data_sz = data_sz - srv->recv_offset;

	int request_sz = reqbuf_get_sane_request_sz(new_data, new_data_sz, 0x81);
	switch(request_sz) {
	case -1: /* format broken */
		log_debug("server %s:%i bad data on the wire", srv->host, srv->port);
		return do_connection_error(uevent, srv, NULL);
	case 0:  /* need more data */
		return uevent_yield(uevent, srv->sd, UEVENT_READ, (uevent_callback_t)do_read, (void*)srv);
	default: /* got full req */
		srv->recv_offset += request_sz;
		srv->responses++;
		if(srv->responses == srv->requests)
			return do_done(uevent, srv);
		goto swallow_next_request;
	}

	// never reached
}

/*
void do_read() {
	read();
	if(request_sz) {
		offset +=;
		response ++;
		if resposnes == {
			return chain(sd, 0, do_done);
		} else {
			
		}
	}
	return yield(sd, READABLE, do_read);
}
*/


static int do_error(struct uevent *uevent, struct st_server *srv) {
	char *res_buf;
	int res_buf_sz;

	buf_get_reader(&srv->recv_buf, &res_buf, &res_buf_sz);
	if(res_buf_sz > srv->recv_offset) {
		buf_rollback_produced(&srv->recv_buf, res_buf_sz - srv->recv_offset);
	}
	/* iterate through every request, again */
	int request_no = 0;
	char *req;
	int req_sz;
	buf_get_reader(&srv->send_buf, &req, &req_sz);
	char *req_end = req + req_sz;
	while(req_end - req) {
		int request_sz = MC_GET_REQUEST_SZ(req);
		
		if(request_no++ >= srv->responses) {
			buf_get_writer(&srv->recv_buf, &res_buf, &res_buf_sz, MAX_REQUEST_SIZE);
			char *err_str = NULL; // default response
			if(MEMCACHE_CMD_NOOP == MC_GET_OPCODE(req))
				err_str = ""; // length 0 so that answer is 24 bytes
			
			int produced = error_from_reqbuf(req, request_sz,
							res_buf, res_buf_sz,
							MEMCACHE_STATUS_CONNECTION_BROKEN,
							err_str);
			
			buf_produce(&srv->recv_buf, produced);
		}
		req += request_sz;
	}
	return do_done(uevent, srv);
}

static int do_done(struct uevent *uevent, struct st_server *srv) {
	char *req;
	int req_sz;
	buf_get_reader(&srv->send_buf, &req, &req_sz);
	buf_consume(&srv->send_buf, req_sz);

	srv->requests = 0;
	srv->responses = 0;
	srv->send_offset = 0;
	srv->recv_offset = 0;
	return 0;
}

static int do_connection_error(struct uevent *uevent, struct st_server *srv, struct timespec *now) {
	struct timespec t0;
	if(NULL == now) {
		clock_gettime(CLOCK_MONOTONIC, &t0);
		now = &t0;
	}
	if(srv->sd >= 0) {
		close(srv->sd);
		srv->sd = -1;
	}
	log_debug("server %s:%i is broken, next retry %.3fs", srv->host, srv->port, ((float)srv->delay)/1000000000.0);
	
	struct timespec prev = srv->next_retry;
	TIMESPEC_ADD(srv->next_retry, prev, srv->delay);
	srv->delay = MIN(srv->delay*2, ST_SERVER_MAX_DELAY);
	return do_error(uevent, srv);
}





