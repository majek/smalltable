#ifndef htonll
#if __BYTE_ORDER == __BIG_ENDIAN
# define htonll(x)       (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define htonll(x)      __bswap_64 (x)
#else
# error Define __BYTE_ORDER
#endif
#endif

#ifndef ntohll
# define ntohll(x) htonll(x)
#endif

int net_bind(char *host, int port);
int net_accept(int sd, char **host, int *port);
