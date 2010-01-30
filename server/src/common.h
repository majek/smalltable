
#define SWAP(a,b)		\
	do{			\
		__typeof (a) c;	\
		c = (a);	\
		(a) = (b);	\
		(b) = (c);	\
	}while(0)

#define MIN(a,b)		\
	({ typeof (a) _a = (a);	\
	   typeof (b) _b = (b);	\
	   _a <= _b ? _a : _b; })

#define MAX(a,b)		\
	({ typeof (a) _a = (a);	\
	   typeof (b) _b = (b);	\
	   _a >= _b ? _a : _b; })


#define NELEM(x) (sizeof(x)/sizeof((x)[0]))


void fatal(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

#define log_debug(format, ...)	global_log("DEBUG", format, ##__VA_ARGS__)
#define log_info(format, ...)	global_log("INFO", format, ##__VA_ARGS__)
#define log_warn(format, ...)	global_log("WARN", format, ##__VA_ARGS__)
#define log_error(format, ...)	global_error(__FILE__, __LINE__, "ERROR", format, ##__VA_ARGS__)
#define log_perror(format, ...)	global_log_perror("ERROR", format, ##__VA_ARGS__)

void global_log_perror(const char *type, const char *s, ...)  __attribute__ ((format (printf, 2, 3)));
void global_log(const char *type, const char *s, ...)  __attribute__ ((format (printf, 2, 3)));
void global_error(const char *filename, int line, const char *type, const char *fmt, ...)  __attribute__ ((format (printf, 4, 5)));

unsigned long hash_djb2(char *s);
unsigned long hash_kr(char *p);
unsigned long hash_sum(char *str);
int key_escape(char *dst, int dst_sz, char *key, int key_sz);

char *read_full_file(char *filename);
