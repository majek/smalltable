

struct connection {
	char *host;
	int port;
	
	struct meta_event event;

	struct buffer recv_buf;
	struct buffer send_buf;

	int quiet_recv_off;
	int requests;
	
	struct server *server;
	struct list_head list;
};

typedef struct connection CONN;

CONN *conn_new(int cd, char *host, int port, struct server *server);
void conn_recv(CONN *conn);
void conn_send(CONN *conn);

int conn_stop(struct server *server, CONN *exception);
int conn_start(struct server *server);
int conn_free_all(struct server *server);
