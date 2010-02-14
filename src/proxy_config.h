
int config_to_string(struct config *config, char *org_buf, int org_buf_sz);
char *load_config_from_string(struct config *config, char *buf, int testing);
void flush_config(struct config *config);
void load_config(struct config *config);
int save_config(struct config *config);


