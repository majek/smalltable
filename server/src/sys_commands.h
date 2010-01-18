void builtin_commands_callback(CONN *conn, char *req_buf, int req_buf_sz,
				struct buffer *send_buf,
				int cmd_flags, void *cmd_fun_ptr);

ST_RES *set_error_code(ST_RES *res, unsigned char status);

ST_RES *cmd_unknown(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);
ST_RES *cmd_get(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);
ST_RES *cmd_set(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);
ST_RES *cmd_delete(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);
ST_RES *cmd_noop(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);
ST_RES *cmd_version(ST_STORAGE_API *api, ST_REQ *req, ST_RES *res);

