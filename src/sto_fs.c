#define _GNU_SOURCE // for O_NOATIME
#define _BSD_SOURCE // nsec for stat

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>

#include "common.h"
#include "storage.h"

struct fs_data{
	char *dir; // MAX_DIR_NAME
};

typedef struct fs_data FS_DATA;


#define MAX_SYSTEM_FILENAME (251)

#define MAX_DIRNAME (128)
#define MAX_FILENAME (MAX_KEY_SIZE*3 + 4) //  +1 is needed for \0
#define MAX_FULLPATH (MAX_DIRNAME + 4+4 + MAX_FILENAME + 1) //directory/000/000/filename

#ifndef O_NOATIME
#define O_NOATIME 0x0
#endif

static char *key_to_filename(char *key, int key_sz, int *a, int *b) {
	static char nkey[MAX_FILENAME];
	int nkey_sz = key_escape(nkey, sizeof(nkey)-1, key, key_sz);
	nkey[nkey_sz] = '\0';
	assert(nkey_sz+1 <= sizeof(nkey));
	
	*a = hash_kr(nkey) % 251;
	*b = hash_sum(nkey) % 239;

	return nkey;
}

static char *key_to_fullpath(char *dir, char *key, int key_sz) {
	static char path[MAX_FULLPATH];
	int a, b;
	char *filename = key_to_filename(key, key_sz, &a, &b);
	
	if(strlen(filename) > MAX_SYSTEM_FILENAME) {
		log_error("key filename stripped. %i bytes instead of %i",
				(int)strlen(filename), MAX_SYSTEM_FILENAME);
		while(strlen(filename) > MAX_SYSTEM_FILENAME) {
			char *c = strrchr(filename, '%');
			if(NULL == c)
				break;
			*c = '\0';
		}
	}
	
	snprintf(path, sizeof(path), "%s/%03i/%03i/%.*s",
				dir, a, b, MAX_SYSTEM_FILENAME, filename);
	return(path);
}

// -1 -> not exists
int fs_get(void *storage_data, char *dst, int size, char *key, int key_sz){
	FS_DATA *fsd = (FS_DATA *)storage_data;

	char *path = key_to_fullpath(fsd->dir, key, key_sz);
	int fd = open(path, O_RDONLY|O_NOATIME);
	if(-1 == fd)
		return(-1);
	
	u_int64_t file_size;
	if(-1 == fd_size(fd, &file_size))
		return(-1);
	if(size < file_size) /* buffer too small */
		return(-1);
	int ret = read(fd, dst, file_size);
	close(fd);
	return(ret);
}

// -1 -> not exits
// else: removed
int fs_del(void *storage_data, char *key, int key_sz) {
	FS_DATA *fsd = (FS_DATA *)storage_data;
	char *path = key_to_fullpath(fsd->dir, key, key_sz);

	int ret = unlink(path);
	if(ret < 0)
		return(-1);
	return(0);
}

// written bytes
// -1 -> not saved
int fs_set(void *storage_data, char *value, int value_sz, char *key, int key_sz) {
	FS_DATA *fsd = (FS_DATA *)storage_data;
	
	char *path = key_to_fullpath(fsd->dir, key, key_sz);
	char path_new[MAX_FULLPATH + 4 ];
	snprintf(path_new, sizeof(path_new), "%s.new", path);
	
	int fd = open(path_new, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	if(fd == -1 && errno == ENOENT) {
		int a, b;
		key_to_filename(key, key_sz, &a, &b);
		char dir1[MAX_DIRNAME+4];
		char dir2[MAX_DIRNAME+4+4];
		snprintf(dir1, sizeof(dir1), "%s/%03i", fsd->dir, a);
		snprintf(dir2, sizeof(dir2), "%s/%03i/%03i", fsd->dir, a, b);
		if(mkdir(dir2, S_IRWXU) != 0 && errno != EEXIST) {
			if(errno == ENOENT) {
				if(mkdir(dir1, S_IRWXU) != 0 && errno != EEXIST) {
					log_perror("mkdir(%s)", dir1);
					return(-1);
				}
				if(mkdir(dir2, S_IRWXU) != 0 && errno != EEXIST) {
					log_perror("mkdir(%s)", dir2);
					return(-1);
				}
			} else {
				log_perror("mkdir(%s)", dir2);
				return(-1);
			}
		}
		fd = open(path_new, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	}
	if(fd == -1) {
		log_perror("open(%s)", path_new);
		return(-1);
	}
	
	int written = 0;
	while(written != value_sz){
		int ret = write(fd, value+written, value_sz-written);
		if(ret < 1) {
			log_perror("write()");
			close(fd);
			return(-1);
		}
		written += ret;
	}
	
	/*
	// Tso is not right. This is inacceptable slow.
	if(fsync(fd) < 0) {
		close(fd);
		perror("fsync()");
		return(-1);
	}
	*/
	close(fd);
	if(rename(path_new, path) != 0) {
		log_perror("rename(%s, %s)", path_new, path);
		return(-1);
	}
	return(written);
}

void fs_sync(void *storage_data) {
	sync();
}

static void fs_readahead(void *storage_data, char **keys, int *keys_sz, int counter) {
	FS_DATA *fsd = (FS_DATA *)storage_data;
	int i;
	for(i=0; i < counter; i++) {
		char *path = key_to_fullpath(fsd->dir, keys[i], keys_sz[i]);
		int fd = open(path, O_RDONLY|O_NOATIME|O_NONBLOCK);
		if(-1 == fd) {
			continue;
		}
		
		posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);
		// well, I'm not really sure if closing just after fadvise
		// is a good idea. It can have no effect at all. Oh well, at
		// least we're going to cache open() metadata.
		// On the other hand kernel seems to enqueue the request,
		// so there is a probability that fadvise() will do the work.
		close(fd);
	}
}

static int get_filename_i_cas(char *path, u_int64_t *i_cas_ptr) {
	struct stat st;
	if(stat(path, &st) != 0) { // never
		log_perror("stat()");
		return(-1);
	}
	*i_cas_ptr = st.st_size << 40 | st.st_mtime << 32 | st.st_mtim.tv_nsec;
	return(0);
}



struct PACKED key_item {
	u_int64_t i_cas;
	u_int8_t key_sz;
	char key[];
};

static int fs_get_keys_traverse_a(FS_DATA *fsd, int a, int b, char *filename, char **buf, int *buf_sz);
static int fs_get_keys_traverse_b(char *dir, int b, char *filename, char **buf, int *buf_sz);
static int fs_get_keys_traverse_c(char *dir, char *filename, char **buf_ptr, int *buf_sz_ptr);

static int fs_get_keys(void *storage_data, char *buf, int buf_sz, char *start_key, int start_key_sz) {
	FS_DATA *fsd = (FS_DATA *)storage_data;
	
	int a = 0;
	int b = 0;
	char *filename = NULL;
	if(start_key_sz){
		
		filename = key_to_filename(start_key, start_key_sz, &a, &b);
	}
	
	char *u_buf = buf;
	int u_buf_sz = buf_sz;
	
	fs_get_keys_traverse_a(fsd, a, b, filename, &u_buf, &u_buf_sz);
	return(u_buf - buf);
}

static int fs_get_keys_traverse_a(FS_DATA *fsd, int a, int b, char *filename, char **buf, int *buf_sz) {
	char dir1[MAX_DIRNAME+4];
	snprintf(dir1, sizeof(dir1), "%s/[0-9][0-9][0-9]", fsd->dir);
		
	glob_t globbuf;
	globbuf.gl_offs = 1;
	glob(dir1, 0, NULL, &globbuf);
	char **off;
	for(off=globbuf.gl_pathv; off && *off; off++) {
		if(!is_dir(*off)) { // never
			log_error("\"%s\" is not a dir, ignored", *off);
			continue;
		}
		int read_a = atoi(*off + (strlen(*off)-3));
		if(read_a >= a) {
			int r;
			if(read_a == a) {
				r = fs_get_keys_traverse_b(*off, b, filename, buf, buf_sz);
			} else {
				r = fs_get_keys_traverse_b(*off, -1, NULL, buf, buf_sz);
			}
			if(-1 == r)
				return(-1);
		}
	}
	globfree(&globbuf);
	return 0;
}

static int fs_get_keys_traverse_b(char *dir, int b, char *filename, char **buf, int *buf_sz) {
	char dir2[MAX_DIRNAME+4+4];
	snprintf(dir2, sizeof(dir2), "%s/[0-9][0-9][0-9]", dir);
	
	glob_t globbuf;
	globbuf.gl_offs = 1;
	glob(dir2, 0, NULL, &globbuf);
	char **off;
	for(off=globbuf.gl_pathv; off && *off; off++) {
		if(!is_dir(*off)) { // never
			log_error("\"%s\" is not a dir, ignored", *off);
			continue;
		}

		int read_b = atoi(*off + (strlen(*off)-3));
		if(read_b >= b) {
			int r;
			if(read_b == b) {
				r = fs_get_keys_traverse_c(*off, filename, buf, buf_sz);
			} else {
				r = fs_get_keys_traverse_c(*off, NULL, buf, buf_sz);
			}
			if(-1 == r)
				return(-1);
		}
	}
	globfree(&globbuf);
	return 0;
}

static int fs_get_keys_traverse_c(char *dir, char *filename, char **buf_ptr, int *buf_sz_ptr) {
	char dir2[MAX_DIRNAME+4+4];
	snprintf(dir2, sizeof(dir2), "%s/*", dir);
	
	glob_t globbuf;
	globbuf.gl_offs = 1;
	glob(dir2, 0, NULL, &globbuf);
	char **off;
	for(off=globbuf.gl_pathv; off && *off; off++) {
		char *curr_filename = strrchr(*off, '/') + 1;
		if(NULL != filename) {
			if(0 == strcmp(curr_filename, filename)) {
				filename = NULL;
			}
			continue;
		}
		char key[MAX_KEY_SIZE];
		int key_sz;
		key_sz = key_unescape(curr_filename, key, sizeof(key));
		if(key_sz < 0) { // never
			log_error("file \"%s\" has broken name. key ignored.", *off);
			continue;
		}
		int item_sz = sizeof(struct key_item) + key_sz;
		if(*buf_sz_ptr < item_sz)
			return -1;
		struct key_item *item = (struct key_item*)*buf_ptr;
		item->key_sz = key_sz;
		if(0 != get_filename_i_cas(*off, &item->i_cas)) { // never
			log_error("file \"%s\" is broken. key ignored.", *off);
			continue;
		}
		memcpy(item->key, key, key_sz);
		*buf_ptr += item_sz;
		*buf_sz_ptr -= item_sz;
	}
	globfree(&globbuf);
	return 0;
}


ST_STORAGE_API *storage_fs_create(int argc, char **argv) {
	if(argc < 1)
		fatal("Please specify database directory.");
	if(argc > 1)
		fatal("Only one directory parameter allowed.");
	char *dir = argv[0];
	if(!is_dir(dir)) {
		mkdir(dir, S_IRWXU);
		if(!is_dir(dir))
			fatal("Directory \"%s\" doesn't exist!", dir);
	}
	
	log_info("FS storage started with root '%s'", dir);
	
	FS_DATA *fsd = (FS_DATA *)calloc(1, sizeof(FS_DATA));
	fsd->dir = strdup(dir);
	ST_STORAGE_API *api = (ST_STORAGE_API *)calloc(1, sizeof(ST_STORAGE_API));
	api->get = &fs_get;
	api->set = &fs_set;
	api->del = &fs_del;
	api->sync = &fs_sync;
	api->readahead = &fs_readahead;
	api->storage_data = fsd;
	api->get_keys = &fs_get_keys;
	return(api);
}

void storage_fs_destroy(ST_STORAGE_API *api) {
	FS_DATA *fsd = (FS_DATA *)api->storage_data;
	free(fsd->dir);
	free(fsd);
	free(api);
}
