#define FUSE_USE_VERSION 31
#define MAX_USERS 1000
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "vfs.h"

static int vfs_pid = -1;

user_info users_list[64];
int users_count = 0;


int get_user_list() {
        struct passwd* p;
        printf("Список пользователей: \n");

        while ((p = getpwent()) != NULL) {
                printf("%s\n", p->pw_name);
				users_count++;
        }
	endpwent();
	return 0;	
}

void free_users_list() {
    if (users_list) {
        for (int i = 0; i < users_count; i++) {
			free(users_list[i].name);
			free(users_list[i].path);
			free(users_list[i].content);
			free(users_list[i].home);
			free(users_list[i].shell);
		}
        users_count = 0;
    }
}


static int users_readdir(
		const char* path,
		void* buf,
		fuse_fill_dir_t filler,
		off_t offset,
		struct fuse_file_info* fi,
		enum fuse_readdir_flags flags
		) {
	(void) offset;
	(void) fi;	
	(void) flags;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	if (strcmp(path, "/Users/arseny/Desktop/University_Programming/TestDocker/users") == 0) {
		for (int i = 0; i < users_count; i++) {
			filler(buf, users_list[i].name, NULL, 0, 0);
		}
		return 0;
	}

	struct user_info *user = find_user_by_path(path);
	if (user) {
		filler(buf, "id", NULL, 0, 0);
		filler(buf, "home", NULL, 0, 0);
		filler(buf, "shell", NULL, 0, 0);
		return 0;
	}

	return -ENOENT;
}


int endswith(const char *str, const char *suffix) {
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) {
        return 0;
	}
    return strcmp(str + lenstr - lensuffix, suffix) == 0;
}

static int users_open(const char* path, struct fuse_file_info* fi) {
    struct user_info* user = find_user_by_path(path);

	if(user && strcmp(path, user->path) == 0) {
		return -EISDIR;
	}

	if (user) {
		if (endswith(path, "/id") || endswith(path, "/home") || endswith(path, "/shell")) {
			return 0;
		}
	}

	return -ENOENT;
}	

static int users_read(
		const char *path, 
		char *buf, 
		size_t size,
		off_t offset,
		struct fuse_file_info *fi
		) {

    (void) fi;
	struct user_info* user = find_user_by_path(path);
	if (!user) {
		return -ENOENT;
	}

	char content[512];
	if (endswith(path, "/id")) {
		snprintf(content, sizeof(content), "%d\n", user->uid);
	} else if (endswith(path, "/home")) {
		snprintf(content, sizeof(content), "%s\n", user->home);
	} else if (endswith(path, "/shell")) {
		snprintf(content, sizeof(content), "%s\n", user->shell);
	} else {
		return -ENOENT;
	}

	size_t length = strlen(content);
	if (offset < length) {
        if (offset + size > length)
            size = length - offset;

        memcpy(buf, content + offset, size);
    } else {
        size = 0;
    }

    return size;
}

static int users_getattr(const char* path, struct stat *stbuf, struct fuse_file_info *fi) {
	(void) fi;

	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0 || strcmp(path, "home") == 0) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

	struct user_info* user = find_user_by_path(path);
	if (user) {
		char extended[512];
		snprintf(extended, sizeof(extended), "/users/%s", user->name);

		if (strcmp(path, extended) == 0) {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
            return 0;
        }

		if (endswith(path, "/id") || endswith(path, "/home") || endswith(path, "/shell")) {
			stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = 128;
            return 0;
		}
	}

	return -ENOENT;
}

static int users_mkdir(const char* path, mode_t mode) {
	const char *username = path + 1;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "adduser --disabled-password --gecos \"\" %s", username);

    int ret = system(cmd);
    if (ret != 0) {
		return -ENOENT;
	} 

    return 0;
}

static struct fuse_operations users_oper = {
	.getattr = users_getattr, 
	.open = users_open,
	.read = users_read, 
	.readdir = users_readdir,
	.mkdir = users_mkdir,
};


int start_users_vfs(const char* mount_point) {
	int pid = fork();
	if (pid == 0) {
		char* fuse_argv[] = {
			"users_vfs",
			"-f",
			"-s",
			(char*) mount_point,
			NULL,
		};

		if (get_user_list() <= 0) {
			//fprintf(stderr, "Can not get list of users\n");
			exit(1);
		}

		mkdir(mount_point, 0755);

		for (int i = 0; users_list[i].name != NULL; i++) {
			char path[512];
			snprintf(path, sizeof(path), "%s/%s", mount_point, users_list[i].name);
			mkdir(path, 0755);
		}

		// int ret = fuse_main(4, fuse_argv, &users_oper, NULL);

		// free_users_list();
		// exit(1);
	} else {

		int vfs_pid = pid;
		//printf(stderr, "VFS started in process %d, in %s\n", pid, mount_point);
		return 0;
	}
}


void stop_users_vfs() {
	if (vfs_pid != -1) {
		kill(vfs_pid, SIGTERM);
		waitpid(vfs_pid, NULL, 0);
		vfs_pid = -1;
		//printf(stderr, "VFS stopped\n");
		
	}
}

