#define _GNU_SOURCE // for O_NOATIME

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

static inline int get_fd_size(int fd, u_int64_t *size) {
	struct stat st;
	if(fstat(fd, &st) != 0){
		log_perror("stat()");
		return(-1);
	}
	if(size)
		*size = st.st_size;
	return(0);
}



char *key_to_filename(char *key, int key_sz) {
	static char nkey[MAX_FILENAME];
	int nkey_sz = key_escape(nkey, sizeof(nkey)-1, key, key_sz);
	nkey[nkey_sz] = '\0';
	assert(nkey_sz+1 <= sizeof(nkey));
	return nkey;
}

char *key_to_fullpath(char *dir, char *key, int key_sz) {
	static char path[MAX_FULLPATH];
	char *filename = key_to_filename(key, key_sz);

	int a = hash_kr(filename) % 251;
	int b = hash_sum(filename) % 239;
	
	if(strlen(filename) > MAX_SYSTEM_FILENAME) {
		log_error("key filename stripped. %i bytes instead of %i", 
			MAX_SYSTEM_FILENAME,
			(int)strlen(filename));
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
	if(-1 == get_fd_size(fd, &file_size))
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
	
	int fd = open(path_new, O_WRONLY|O_TRUNC|O_CREAT|O_NOATIME, S_IRUSR|S_IWUSR);
	if(fd == -1 && errno == ENOENT) {
		char *filename = key_to_filename(key, key_sz);
		char dir1[MAX_DIRNAME+4];
		char dir2[MAX_DIRNAME+4+4];
		int a = hash_kr(filename) % 251;
		int b = hash_sum(filename) % 239;
		snprintf(dir1, sizeof(dir1), "%s/%03i", fsd->dir, a);
		snprintf(dir2, sizeof(dir2), "%s/%03i/%03i", fsd->dir, a, b);
		if(mkdir(dir1, S_IRWXU) != 0 && errno != EEXIST) {
			perror("mkdir()");
			return(-1);
		}
		if(mkdir(dir2, S_IRWXU) != 0 && errno != EEXIST) {
			perror("mkdir()");
			return(-1);
		}
		fd = open(path_new, O_WRONLY|O_TRUNC|O_CREAT|O_NOATIME, S_IRUSR|S_IWUSR);
	}
	if(fd == -1) {
		perror("open()");
		return(-1);
	}
	
	int written = 0;
	while(written != value_sz){
		int ret = write(fd, value+written, value_sz-written);
		if(ret < 1) {
			perror("write()");
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
		perror("rename()");
		return(-1);
	}
	return(written);
}

void fs_sync(void *storage_data) {
	sync();
}

static void fs_readahead(void *storage_data, char **keys, int *key_sz, int counter) {

}

int is_dir(char *path) {
	struct stat st;
	if(0 != stat(path, &st)) {
		return(0);
	}
	if(!S_ISDIR(st.st_mode))
		return(0);
	return(1);
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
	return(api);
}

void storage_fs_destroy(ST_STORAGE_API *api) {
	FS_DATA *fsd = (FS_DATA *)api->storage_data;
	free(fsd->dir);
	free(fsd);
	free(api);
}
