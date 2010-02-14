#ifndef _COMMON_H
#define _COMMON_H

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

#ifndef PACKED
#define PACKED __attribute__ ((packed))
#endif

#define _NANO 1000000000LL
#define _MICRO 1000000LL
#define _MILLI 1000LL

/* Timespec subtraction in nanoseconds */
#define TIMESPEC_NSEC_SUBTRACT(a,b) (((a).tv_sec - (b).tv_sec) * _NANO + (a).tv_nsec - (b).tv_nsec)
/* Timespec subtract in milliseconds */
#define TIMESPEC_MSEC_SUBTRACT(a,b) ((((a).tv_sec - (b).tv_sec) * _MILLI) + ((a).tv_nsec - (b).tv_nsec) / _MICRO)
/* Timespec subtract in seconds; truncate towards zero */
#define TIMESPEC_SEC_SUBTRACT(a,b) ((a).tv_sec - (b).tv_sec + (((a).tv_nsec < (b).tv_nsec) ? -1 : 0))

#define TIMESPEC_BEFORE(a, b) (((a).tv_sec < (b).tv_sec) || ((a).tv_sec == (b).tv_sec && (a).tv_nsec < (b).tv_nsec))

#define TIMESPEC_ADD(a, b, nsecs) { \
		(a).tv_sec = (b).tv_sec + ((nsecs) / _NANO); \
		(a).tv_nsec = (b).tv_nsec + ((nsecs) % _NANO); \
		(a).tv_sec += (a).tv_nsec / _NANO; \
		(a).tv_nsec %= _NANO; \
	}


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
int key_unescape(char *src, char *key, int key_sz);

int is_dir(char *path);
int fd_size(int fd, u_int64_t *size);
char *read_full_file(char *filename);

char *encode_hex(char *key, int key_sz);
int decode_hex(char **key_ptr, int *key_sz_ptr, char *hex);

#endif // _COMMON_H
