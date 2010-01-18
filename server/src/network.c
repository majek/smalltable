#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "coverage.h"


void net_set_ridiculously_high_buffers(int cd) {
	int i, j;
	int flags[] = {SO_SNDBUF, SO_RCVBUF};
	for(j=0; j<sizeof(flags)/sizeof(flags[0]); j++) {
		int flag = flags[j];
		for(i=0; i<10; i++) {
			int bef;
			socklen_t size = sizeof(bef);
			if(getsockopt(cd,SOL_SOCKET, flag, &bef, &size)<0) {
				log_perror("getsockopt()");
				break;
			}
			int opt = bef*2;
			if(setsockopt(cd,SOL_SOCKET, flag, &opt, sizeof(opt))<0)
				break;
			int aft;
 			size = sizeof(aft);
 			if(getsockopt(cd,SOL_SOCKET, flag, &aft, &size)<0) {
				break;
			}
			if(aft <= bef || aft >= 16777216) // until it increases
				break;
		}
	}
}

void set_nonblocking(int fd) {
	int flags, ret;
	flags = fcntl(fd, F_GETFL, 0);
	if (NEVER(-1 == flags))
		flags = 0;
	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if(NEVER(ret < 0))
		log_perror("fcntl()");
}

int net_gethostbyname(char *host, in_addr_t *s_addr) {
	if(NEVER(!s_addr || !host))
		return(-1);

	if(strcmp(host, "") == 0 || strcmp(host, "*") == 0 || strcmp(host, "0.0.0.0") == 0) {
		*s_addr = INADDR_ANY;
		return(0);
	}
	struct addrinfo *result;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int error = getaddrinfo(host, NULL, &hints, &result);
	if(error != 0)
		goto error;
	/* loop over all returned results */
	struct addrinfo * res;
	for (res = result; res != NULL; res = res->ai_next) {
		if(res->ai_family == AF_INET){
			struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
			*s_addr = sin->sin_addr.s_addr;
			return(0);
		}
	}
error:
	log_error("getaddrinfo(): host '%s' not found", host);
	return(-1);
}

int net_bind(char *host, int port) {
	int r;
	int sd = socket(PF_INET, SOCK_STREAM, 0);
	if(NEVER(sd < 0)) {
		log_perror("socket()");
		return(-1);
	}
	
	int opt = 1;
	r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(NEVER(r < 0)) {
		log_perror("setsockopt()");
		goto error;
	}
	
	in_addr_t s_addr;
	if(net_gethostbyname(host, &s_addr) < 0) {
		goto error;
	}
	
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = s_addr;
	sin.sin_port=htons(port);
	if(bind(sd, (void *)&sin, sizeof(sin)) < 0) {
		log_perror("bind('%s', %i)", host, port);
		goto error;
	}
	
	r = listen(sd, 32);
	if(NEVER(r < 0)) {
		log_perror("listen()");
		goto error;
	}
	
	set_nonblocking(sd);
	return(sd);
error:
	close(sd);
	return(-1);
}

int net_accept(int sd, char **host, int *port) {
	struct sockaddr_in sin;
	socklen_t sinlen = sizeof(struct sockaddr_in);
	int cd = accept(sd, (struct sockaddr*)&sin, &sinlen);
	assert(sin.sin_family == AF_INET);
	if(host)
		*host = inet_ntoa(sin.sin_addr);
	if(port)
		*port = sin.sin_port;
	
	net_set_ridiculously_high_buffers(cd);
	set_nonblocking(cd);
	return(cd);
}





