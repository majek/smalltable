
enum {
	META_EVENT_REGISTER,
	META_EVENT_UNREGISTER,
	META_EVENT_ONLY_READ,
	META_EVENT_ONLY_WRITE,
};

struct meta_event {
	int cd;
	ev_callback_t callback;
	struct event ev;
	
	void *userdata;
	
	int flag_write_only;
	int flag_registered;
};

struct server {
	int sd;
	struct event ev;

	char *host;
	int port;

	char ping_parent;
	char trace;
	
	void (*info_handler)(void *userdata);
	void (*quit_handler)(void *userdata);
	int (*process_multi)(struct connection *conn, char *requests, int requests_sz);
	void *userdata;

	int stopped;
	struct list_head root;
};


void modify_event(struct meta_event *me, int action);
void client_callback(int cd, short event, void *udata);

void do_event_loop(struct server *server);

