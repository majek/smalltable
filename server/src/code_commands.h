ST_RES *cmd_code_load(CONN *conn, ST_REQ *req, ST_RES *res);
ST_RES *cmd_code_unload(CONN *conn, ST_REQ *req, ST_RES *res);
ST_RES *cmd_code_check(CONN *conn, ST_REQ *req, ST_RES *res);

int process_test_str(struct config *config, char *value, int value_sz, char *keyprefix);

/* public but from process.c */
void process_initialize(struct config *config);
void process_destroy();

void process_commands_callback(CONN *conn, char *req, int req_sz, 
				struct buffer *send_buf,
				int cmd_flags, void *process_ud);

