#ifndef _SHARED_H
#define _SHARED_H

#include <sys/types.h>
#include <event.h>
/* derived from libevent */
typedef void (*ev_callback_t)(int,short,void*);

#include "list.h"


#include "coverage.h"

struct config;
struct connection;

#include "constants.h"

#include "common.h"
#include "buffer.h"
#include "event_loop.h"
#include "network.h"

#include "connection.h"
#include "framing.h"


#endif // _SHARED_H
