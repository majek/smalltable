
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

void modify_event(struct meta_event *me, int action);
void client_callback(int cd, short event, void *udata);

void do_event_loop(struct server *server);

