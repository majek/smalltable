#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "st.h"

#define EVENT_MAX_TIMEOUT {0x0FFFFFFF, 0}
static void meta_event_add(struct meta_event *me) { /* Edge driven. */
	int flags;
	if(0 == me->flag_write_only) {
		flags = EV_READ|EV_PERSIST;
	} else {
		flags = EV_WRITE|EV_PERSIST;
	}
	
	struct timeval tv = EVENT_MAX_TIMEOUT;
	event_set(&me->ev, me->cd, flags, me->callback, me->userdata);
	event_add(&me->ev, &tv);
}

static void meta_event_del(struct meta_event *me) { /* Edge driven. */
	event_del(&me->ev);
}


/* abstract out event_add/del calls, to make sure they're runned once.
 Level driven. */
void modify_event(struct meta_event *me, int action) {
	switch(action) {
	case META_EVENT_REGISTER:
		if(0 == me->flag_registered) {
			me->flag_registered = 1;
			meta_event_add(me);
		}
		break;
	case META_EVENT_UNREGISTER:
		if(1 == me->flag_registered) {
			me->flag_registered = 0;
			meta_event_del(me);
		}
		break;
	case META_EVENT_ONLY_WRITE:
		if(0 == me->flag_write_only) {
			me->flag_write_only = 1;
			if(1 == me->flag_registered) {
				meta_event_del(me);
				meta_event_add(me);
			}
		}
		break;
	case META_EVENT_ONLY_READ:
		if(1 == me->flag_write_only) {
			me->flag_write_only = 0;
			if(1 == me->flag_registered) {
				meta_event_del(me);
				meta_event_add(me);
			}
		}
		break;
	#ifndef COVERAGE_TEST
	default:
		log_error("assert(0)");
		assert(0);
	#endif
	}
}

void client_callback(int cd, short event, void *udata) {
	CONN *conn = (CONN *)udata;
	
	if(0 != (event & EV_READ)){
		conn_recv(conn);
		return;
	}
	if(0 != (event & EV_WRITE)){
		conn_send(conn);
		return;
	}
	#ifndef COVERAGE_TEST
	log_error("assert(0)");
	assert(0);
	#endif
}

static void server_callback(int sd, short event, void *sdata) {
	struct server *server = (struct server*)sdata;

	char *host;
	int port;
	int cd = net_accept(sd, &host, &port);
	
	conn_new(cd, host, port, server);
}

static struct server *server_bind(char *host, int port, ev_callback_t handler, struct server *server) {
	log_info("Binding to %s:%i", host, port);
	int sd = net_bind(host, port);
	if(sd < 0) {
		return(NULL);
	}
	
	server->sd = sd;
	
	struct timeval tv = EVENT_MAX_TIMEOUT;
	event_set(&server->ev, sd, EV_READ|EV_PERSIST, server_callback, (void *)server);
	event_add(&server->ev, &tv);
	return(server);
}


static void signal_int(int fd, short event, void *arg) {
	struct server *server = (struct server *)arg;
	log_info("Received CTRL+C, closing");
	storage_sync(server->api);
	event_loopexit(NULL);
}

static void signal_sync(int fd, short event, void *arg) {
	struct server *server = (struct server *)arg;
	log_info("Received signal, syncing database.");
	storage_sync(server->api);
	log_info("Synced.");
	int pool_items, pool_bytes;
	get_pool_size(&pool_items, &pool_bytes);
	log_info("Memory stats: %iMB in %i items in pool. Pool freed.",
				pool_bytes/1024/1024, pool_items);
	pool_free();
}

static struct event *catch_signal(int sig_no, ev_callback_t handler, struct server *server) {
	struct event *ev= (struct event*)st_calloc(1, sizeof(struct event));
	struct timeval tv = EVENT_MAX_TIMEOUT;

	event_set(ev, sig_no, EV_SIGNAL|EV_PERSIST, handler, (void*)server);
	event_add(ev, &tv);
	return(ev);
}

static void ev_close(struct event *ev, int fd) {
	if(ev) {
		event_del(ev);
	}
	if(fd > 0)
		close(fd);
}

void do_event_loop(char *host, int port, struct server *server) {
	struct server *srv;
	struct event *sigev[5];
	log_info("Libevent version: %s", event_get_version());
	event_init();

	sigev[0] = catch_signal(SIGINT,  &signal_int, server);
	sigev[1] = catch_signal(SIGTERM, &signal_int, server);
	sigev[2] = catch_signal(SIGHUP, &signal_sync, server);
	sigev[3] = catch_signal(SIGUSR1, &signal_sync, server);
	sigev[4] = catch_signal(SIGUSR2, &signal_sync, server);


	srv = server_bind(host, port, server_callback, server);
	if(server->ping_parent) {
		kill(getppid(), SIGURG);
	}

	if(!srv) {
		log_perror("Can't bind to socket %s:%i", host, port);
	} else {
		int r = event_dispatch();
		if(NEVER(r != 0))
			log_error("event_dispatch() = %i", r);
	}
	int i;
	for(i=0; i<sizeof(sigev)/sizeof(sigev[0]); i++)
		ev_close(sigev[i], -1);
	if(srv)
		ev_close(&srv->ev, srv->sd);
}
