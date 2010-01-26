#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#include "shared.h"
#include "proxy.h"

extern char *optarg;

static void info_handler(void *arg) {
	//struct config *config = (struct config*)arg;
	log_info("Received info request.");
	int pool_items, pool_bytes;
	get_pool_size(&pool_items, &pool_bytes);
	log_info("Memory stats: %iMB in %i items in pool. Pool freed.",
				pool_bytes/1024/1024, pool_items);
	pool_free();
}


void print_help(struct server *server, struct config *config) {
printf(
"Usage: smalltable-proxy [OPTION]...\n"
VERSION_STRING " - a proxy for smalltable key-value server\n"
"\n"
"Usefull options:\n"
"  -l, --listen=HOST         bind to specified hostname (default=%s)\n"
"  -p, --port=PORT           tcp/ip port number to listen on (default=%i)\n"
"  -c, --config=FILE         path to the file that contains automatically\n"
"                            generated configuration (default=%s)\n"
"\n"
"Useless options:\n"
"  -v, --verbose             print more useless debugging messages\n"
"  -x, --ping-parent         send SIGURG to parent pid after successfull binding\n"
"\n",
	server->host,
	server->port,
	config->config_path
);
}

int main(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"listen", required_argument, 0, 'l'},
		{"port", required_argument, 0, 'p'},
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, 0, 'v'},
		{"ping-parent", no_argument, 0, 'x'},
		{"config", required_argument, 0, 'c'},
		{0, 0, 0, 0}
	};
	
	struct server *server = (struct server*)st_calloc(1, sizeof(struct server));
	server->info_handler = &info_handler;
	//server->quit_handler = &quit_handler;
	server->process_multi= &process_multi;

	server->host = "127.0.0.1";
	server->port = 22122;
	
	struct config *config = (struct config*)st_calloc(1, sizeof(struct config));
	server->userdata = config;
	config->config_path = "./smalltable-proxy.config";
	config->proxies = RB_ROOT;
	config->servers = RB_ROOT;
	int option_index;
	int arg;
	while((arg = getopt_long_only(argc, argv, "hvxl:p:c:", long_options, &option_index)) != EOF) {
		switch(arg) {
		case 'h':
			print_help(server, config);
			exit(-1);
			break;
		case 'v':
			server->trace = 1;
			break;
		case 'x':
			server->ping_parent = 1;
			break;
		case 'l':
			server->host = optarg;
			break;
		case 'p':
			server->port = atoi(optarg);
			if(server->port < 0 || server->port > 65536)
				fatal("Port number broken: %i", server->port);
			break;
		case 'c':
			break;
		case 0:
		default:
			fatal("\nUnknown option: \"%s\"\n", argv[optind-1]);
		}
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "%s.new", config->config_path);
	config->config_path_new = strdup(buf);
	
	log_info("Process pid %i", getpid());
	signal(SIGPIPE, SIG_IGN);
	
	load_config(config);
	
	do_event_loop(server);
	
	log_info("Quit");
	
	exit(0);
	return(0);
}
