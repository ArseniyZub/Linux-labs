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
static char g_mount_point[512] = {0};

user_info users_list[128];
int users_count;

int endswith(const char *str, const char *suffix) {
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) {
        return 0;
	}
    return strcmp(str + lenstr - lensuffix, suffix) == 0;
}

int get_user_list() {
	struct passwd* p;

	users_count = 0;
	setpwent();
	//printf("Список пользователей: \n");

	while ((p = getpwent()) != NULL) {
		if (!endswith(p->pw_shell, "sh"))  {
			continue;
		}
		
		users_list[users_count].name = strdup(p->pw_name);
		users_list[users_count].uid = p->pw_uid;
		users_list[users_count].home = strdup(p->pw_dir);
		users_list[users_count].shell = strdup(p->pw_shell);

		users_count++;

		if (users_count >= 128) {
			break;
		}
	}

	endpwent();
	return users_count;
}

void free_users_list() {
    for (int i = 0; i < users_count; i++) {
        free(users_list[i].name);
        free(users_list[i].home);
        free(users_list[i].shell);
    }
    users_count = 0;
}

struct user_info* find_user_by_path(const char* path) {
    if (!path || path[0] != '/')
        return NULL;

    const char* name = path + 1;

    const char* slash = strchr(name, '/');
    size_t len = slash ? (size_t)(slash - name) : strlen(name);

    for (int i = 0; i < users_count; i++) {
        if (strlen(users_list[i].name) == len &&
            strncmp(users_list[i].name, name, len) == 0)
        {
            return &users_list[i];
        }
    }

    return NULL;
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

	if (strcmp(path, "/") == 0) {
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


static int users_open(const char* path, struct fuse_file_info* fi) {
    struct user_info* user = find_user_by_path(path);

	if (user) {
		char user_dir[512];
		snprintf(user_dir, sizeof(user_dir), "/%s", user->name);
		if (strcmp(path, user_dir) == 0) {
			return -EISDIR;
		}
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
        snprintf(content, sizeof(content), "%d\n", user->uid); // Removed trailing newline
    } else if (endswith(path, "/home")) {
        snprintf(content, sizeof(content), "%s\n", user->home); // Removed trailing newline
    } else if (endswith(path, "/shell")) {
        snprintf(content, sizeof(content), "%s\n", user->shell); // Removed trailing newline
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

		if (strcmp(path, extended) == 0 || strcmp(path, user->name) == 0) {
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

	if (!username || strlen(username) == 0) {
        return -EINVAL;
    }

    if ((int)users_count >= (int)(sizeof(users_list)/sizeof(users_list[0]))) {
        return -ENOSPC;
    }

    struct passwd* p;
    uid_t max_uid = 1000;

    setpwent();
    while ((p = getpwent()) != NULL) {
        if (p->pw_uid > max_uid)
            max_uid = p->pw_uid;
    }
    endpwent();

    uid_t new_uid = max_uid + 1;

    char line[512];
    snprintf(line, sizeof(line), "%s:x:%d:%d:%s:/home/%s:/bin/bash\n",
        username, new_uid, new_uid, username, username
    );

    FILE *f = fopen("/etc/passwd", "a");
    if (!f)
        return -errno;

    fputs(line, f);
    fclose(f);

	char home_path[512];
    snprintf(home_path, sizeof(home_path), "/home/%s", username);
	mkdir(home_path, 0755);
	
    users_list[users_count].name = strdup(username);
    users_list[users_count].uid = new_uid;
    users_list[users_count].home = strdup(home_path);
    users_list[users_count].shell = strdup("/bin/bash");
    users_count++;

	if (g_mount_point[0] != '\0') {
        char full_dir[1024];
        snprintf(full_dir, sizeof(full_dir), "%s%s", g_mount_point, path);
        mkdir(full_dir, 0755);

        char file_path[1024];
        FILE *fp;

        snprintf(file_path, sizeof(file_path), "%s/id", full_dir);
        fp = fopen(file_path, "w");
        if (fp) { 
			fprintf(fp, "%d\n", new_uid); fclose(fp); 
		}

        snprintf(file_path, sizeof(file_path), "%s/home", full_dir);
        fp = fopen(file_path, "w");
        if (fp) { 
			fprintf(fp, "%s\n", home_path); fclose(fp); 
		}

        snprintf(file_path, sizeof(file_path), "%s/shell", full_dir);
        fp = fopen(file_path, "w");
        if (fp) { 
			fprintf(fp, "%s\n", "/bin/bash"); fclose(fp); 
		}
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
		setenv("FUSE_NO_OPEN_DEV_FUSE", "1", 1);
		strncpy(g_mount_point, mount_point, sizeof(g_mount_point) - 1);
		g_mount_point[sizeof(g_mount_point) - 1] = '\0';
		char* fuse_argv[] = {
			"users_vfs",
			"-f",
			"-s",
			(char*) mount_point,
			NULL,
		};

		if (get_user_list() <= 0) {
			_exit(1);
		}

		mkdir(mount_point, 0755);

		int ret = fuse_main(4, fuse_argv, &users_oper, NULL);

		free_users_list();
		_exit(ret == 0 ? 0 : 1);
	} else if (pid > 0) {
		vfs_pid = pid;
		return 0;
	} else {
		return -1;
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

