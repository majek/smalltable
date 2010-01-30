#include <sys/select.h>
#include <string.h>
#include <errno.h>

#include "uevent.h"
#include "common.h"

struct uevent *uevent_new(struct uevent *uevent) {
	uevent->used_slots = 0;
	uevent->max_fd = 0;
	FD_ZERO(&uevent->readfds);
	FD_ZERO(&uevent->writefds);
	return(uevent);
}

int uevent_loop(struct uevent *uevent) {
	int counter = 0;
	while(uevent->used_slots) {
		counter++;
		fd_set rfds;
		fd_set wfds;
		memcpy(&rfds, &uevent->readfds, sizeof(rfds));
		memcpy(&wfds, &uevent->writefds, sizeof(wfds));
		int r = select(uevent->max_fd + 1, &rfds, &wfds, NULL, NULL);
		if(-1 == r) {
			if(EINTR == errno)
				continue;
			log_perror("select()");
		}
		int i;
		for(i=0; i < uevent->max_fd+1; i++) {
			int mask = 0;
			if(FD_ISSET(i, &rfds)) {
				mask |= UEVENT_READ;
				FD_CLR(i, &uevent->readfds);
			}
			if(FD_ISSET(i, &wfds)) {
				mask |= UEVENT_WRITE;
				FD_CLR(i, &uevent->writefds);
			}
			if(mask) {
				uevent->used_slots--;
				uevent->fdmap[i].callback(uevent, i, mask, uevent->fdmap[i].userdata);
			}
		}
	}
	return(counter);
}

int uevent_yield(struct uevent *uevent, int fd, int mask, uevent_callback_t callback, void *userdata) {
	if(fd >= __FD_SETSIZE) {
		fatal("Can't handle more than %i descriptors.", __FD_SETSIZE);
	}
	if(mask & UEVENT_READ) {
		FD_SET(fd, &uevent->readfds);
	}
	if(mask & UEVENT_WRITE) {
		FD_SET(fd, &uevent->writefds);
	}
	uevent->fdmap[fd].callback = callback;
	uevent->fdmap[fd].userdata = userdata;
	uevent->max_fd = MAX(uevent->max_fd, fd);
	uevent->used_slots++;
	return(1);
}

