
typedef void (*commands_callback_ptr)(CONN *conn, char *req, int req_sz, 
				struct buffer *send_buf,
				int cmd_flags, void *cmd_ptr);

int command_register(int cmd, int user_flags, void *process_ud);
int command_unregister(int cmd);
void command_get(int cmd, int *flags_ptr, void **process_ud);
int command_find_by_process_ud(void *process_ud);

int process_multi(CONN *conn, char *req_buf, int req_buf_sz);
void commands_initialize();
void commands_destroy();

enum {
	CMD_FLAG_RO		= 1 << 0,	// can't modify this command - can't be reassigned to custom code
	CMD_FLAG_PREFETCH	= 1 << 2,	// automatically prefetch given key
	CMD_FLAG_EXTRA_PARAM	= 1 << 3,	// callback function takes userdata and conn, instead of api
	CMD_FLAG_PROCESS	= 1 << 4	// passes buffer instead of req/res
};

struct cmd_pointers{
	void* ptr;
	int flags;
};
extern struct cmd_pointers cmd_pointers[256];

static inline int cmd_check(u_int8_t opcode, int flag) {
	return (cmd_pointers[opcode].flags & flag) ? 1 : 0;
}


extern u_int64_t unique_number;

