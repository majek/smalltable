#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "shared.h"


CONN *conn_new(int cd, char *host, int port, struct server *server) {
	log_info("%s:%i opened", host, port);
	
	CONN *conn = (CONN *)st_calloc(1, sizeof(CONN));
	conn->host = strdup(host);
	conn->port = port;
	conn->server = server;
	
	conn->event.userdata = (void*)conn;
	conn->event.cd = cd;
	conn->event.callback = &client_callback;

	list_add(&(conn->list), &(server->root));

	modify_event(&conn->event, META_EVENT_ONLY_READ);
	if(0 == server->stopped)
		modify_event(&conn->event, META_EVENT_REGISTER);
	return(conn);
}


void conn_free(CONN *conn) {
	free(conn->host);
	
	modify_event(&conn->event, META_EVENT_UNREGISTER);
	
	close(conn->event.cd);
	
	buf_free(&conn->recv_buf);
	buf_free(&conn->send_buf);
	
	list_del(&(conn->list));

	free(conn);
}

void conn_close(CONN *conn) {
	log_info("%s:%i closed", conn->host, conn->port);
	conn_free(conn);
}


/* Read data from socket, run process_multi() once enough data is collected. */
void conn_recv(CONN *conn) {
	if(NEVER(unlikely(conn->server->trace)))
		log_debug("%s:%i event:recv", conn->host, conn->port);
retry_recv:;
	char *buf;
	int buf_sz;
	buf_get_writer(&conn->recv_buf, &buf, &buf_sz, 0);
	
	errno = 0;
	int ret = recv(conn->event.cd, buf, buf_sz, MSG_DONTWAIT);
	
	if(ret <= 0) {
		if(NEVER(errno == EAGAIN)) {
			/* just wait for next event */
			ret = 0;
		} else {
			if(NEVER(ret < 0)) {
				log_perror("%s:%i recv()", conn->host, conn->port);
			} else {
				/* Normal close. */
			}
			conn_close(conn);
			return;
		}
	}
	buf_produce(&conn->recv_buf, ret);
	
	/* Have we stopped reading because of the end of the buffer? */
	if(ret == buf_sz) {
		goto retry_recv;
	}
	
	int written = 0;
	
swallow_next_request:;
	char *data;
	int data_sz;
	buf_get_reader(&conn->recv_buf, &data, &data_sz);
	
	char *new_data = data + conn->quiet_recv_off;
	int new_data_sz = data_sz - conn->quiet_recv_off;
	
	int request_sz = reqbuf_get_sane_request_sz(new_data, new_data_sz);
	switch(request_sz) {
	case -1: /* format broken */
		conn_close(conn);
		return;
	case 0:  /* request more data */
		if(written) {
			conn_send(conn);
		}
		return;
	default:
		/* enough data, and quiet commands */
		if( (MC_GET_RESERVED(new_data) & MEMCACHE_RESERVED_FLAG_QUIET)
					&& data_sz < MAX_REQUEST_SIZE
					&& conn->requests < MAX_QUIET_REQUESTS) {
			conn->quiet_recv_off += request_sz;
			conn->requests++;
			goto swallow_next_request;
		} else {
			conn->quiet_recv_off += request_sz;
			conn->requests++;
			
			int consumed = \
				conn->server->process_multi(conn, data, conn->quiet_recv_off);
			
			buf_consume(&conn->recv_buf, consumed);
			/* We can assume that there's data to be written. */
			written = 1;
			conn->quiet_recv_off = 0;
			conn->requests = 0;
			
			/* full out buffer. don't process more */
			if(buf_get_used(&conn->send_buf) >= MAX_REQUEST_SIZE) {
				conn_send(conn);
				return;
			}
			goto swallow_next_request;
		}
	}
}

void conn_send(CONN *conn) {
	if(NEVER(unlikely(conn->server->trace)))
		log_debug("%s:%i event:send", conn->host, conn->port);
	char *buf;
	int buf_sz = 0;
	buf_get_reader(&conn->send_buf, &buf, &buf_sz);
	
	int ret = 0;
	if(buf_sz > 0) {
		errno = 0;
		ret = send(conn->event.cd, buf, buf_sz, MSG_DONTWAIT);
		if(ret <= 0) {
			if(NEVER(errno == EAGAIN)) {
				/* Impossible to send ATM. Weird. */
				log_debug("%s:%i write() = EAGAIN", conn->host, conn->port);
				ret = 0;
			} else {
				log_perror("%s:%i send()", conn->host, conn->port);
				conn_close(conn);
				return;
			}
		} else {
			buf_consume(&conn->send_buf, ret);
		}
	}

	if(buf_sz != ret) { // yet to send
		modify_event(&conn->event, META_EVENT_ONLY_WRITE);
	} else { // no more to send
		modify_event(&conn->event, META_EVENT_ONLY_READ);
		if(buf_get_used(&conn->recv_buf)) {
			conn_recv(conn);
		}
	}
}

static int conn__do_for_all(struct server *server, CONN *exception, int flag) {
	struct list_head *pos;
	int i = 0;
	list_for_each(pos, &(server->root)) {
		CONN *conn = list_entry(pos, CONN, list);
		if(conn != exception) {
			i++;
			modify_event(&conn->event, flag);
		}
	}
	return(i);
}

int conn_stop(struct server *server, CONN *exception) {
	server->stopped = 1;
	return conn__do_for_all(server, exception, META_EVENT_UNREGISTER);
}

int conn_start(struct server *server) {
	server->stopped = 0;
	return conn__do_for_all(server, NULL, META_EVENT_REGISTER);
}
