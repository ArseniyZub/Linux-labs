#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "vfs.h"

/* -------------------------------------------------- */

static int vfs_pid = -1;

user_info users[128];
int users_count = 0;

/* -------------------------------------------------- */

static int endswith(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

/* -------------------------------------------------- */

static void load_users() {
    struct passwd *p;
    users_count = 0;

    setpwent();
    while ((p = getpwent()) && users_count < 128) {
        if (!endswith(p->pw_shell, "sh"))
            continue;

        users[users_count].name  = strdup(p->pw_name);
        users[users_count].uid   = p->pw_uid;
        users[users_count].home  = strdup(p->pw_dir);
        users[users_count].shell = strdup(p->pw_shell);
        users_count++;
    }
    endpwent();
}

static user_info *find_user(const char *name) {
    for (int i = 0; i < users_count; i++)
        if (strcmp(users[i].name, name) == 0)
            return &users[i];
    return NULL;
}

/* -------------------------------------------------- */
/* FUSE */

static int vfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    memset(st, 0, sizeof(*st));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    char user[128], file[128];
    if (sscanf(path, "/%127[^/]/%127s", user, file) == 2) {
        if (!find_user(user))
            return -ENOENT;

        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 128;
        return 0;
    }

    if (sscanf(path, "/%127s", user) == 1) {
        if (!find_user(user))
            return -ENOENT;

        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    return -ENOENT;
}

static int vfs_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t off,
    struct fuse_file_info *fi,
    enum fuse_readdir_flags flags
) {
    (void) off;
    (void) fi;
    (void) flags;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    if (strcmp(path, "/") == 0) {
        for (int i = 0; i < users_count; i++)
            filler(buf, users[i].name, NULL, 0, 0);
        return 0;
    }

    char user[128];
    if (sscanf(path, "/%127s", user) == 1 && find_user(user)) {
        filler(buf, "id", NULL, 0, 0);
        filler(buf, "home", NULL, 0, 0);
        filler(buf, "shell", NULL, 0, 0);
        return 0;
    }

    return -ENOENT;
}

static int vfs_open(const char *path, struct fuse_file_info *fi) {
    (void) fi;
    char user[128], file[128];

    if (sscanf(path, "/%127[^/]/%127s", user, file) == 2) {
        if (find_user(user))
            return 0;
    }
    return -ENOENT;
}

static int vfs_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    (void) fi;
    char user[128], file[128];

    if (sscanf(path, "/%127[^/]/%127s", user, file) != 2)
        return -ENOENT;

    user_info *u = find_user(user);
    if (!u)
        return -ENOENT;

    char data[256];

    if (strcmp(file, "id") == 0)
        snprintf(data, sizeof(data), "%d", u->uid);
    else if (strcmp(file, "home") == 0)
        snprintf(data, sizeof(data), "%s", u->home);
    else if (strcmp(file, "shell") == 0)
        snprintf(data, sizeof(data), "%s", u->shell);
    else
        return -ENOENT;

    size_t len = strlen(data);
    if (offset >= len)
        return 0;

    if (offset + size > len)
        size = len - offset;

    memcpy(buf, data + offset, size);
    return size;
}

static int vfs_mkdir(const char *path, mode_t mode) {
    (void) mode;

    const char *name = path + 1;
    if (!name || !*name)
        return -EINVAL;

    if (find_user(name))
        return -EEXIST;

    uid_t max_uid = 1000;
    struct passwd *p;

    setpwent();
    while ((p = getpwent()))
        if (p->pw_uid > max_uid)
            max_uid = p->pw_uid;
    endpwent();

    uid_t uid = max_uid + 1;

    FILE *f = fopen("/etc/passwd", "a");
    if (!f)
        return -EACCES;

    fprintf(
        f,
        "%s:x:%d:%d:%s:/home/%s:/bin/bash\n",
        name, uid, uid, name, name
    );
    fclose(f);

    users[users_count].name  = strdup(name);
    users[users_count].uid   = uid;

    char home[256];
    snprintf(home, sizeof(home), "/home/%s", name);
    users[users_count].home  = strdup(home);
    users[users_count].shell = strdup("/bin/bash");

    users_count++;
    return 0;
}

/* -------------------------------------------------- */

static struct fuse_operations ops = {
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .open    = vfs_open,
    .read    = vfs_read,
    .mkdir   = vfs_mkdir,
};

/* -------------------------------------------------- */

int start_users_vfs(const char *mount_point) {
    load_users();

    int pid = fork();
    if (pid == 0) {
        char *argv[] = {
            "kubsh-vfs",
            "-f",
            (char *)mount_point,
            NULL
        };
        fuse_main(3, argv, &ops, NULL);
        exit(0);
    }

    vfs_pid = pid;
    printf("Vfs запущена в процессе %d и смонтирована в %s\n", pid, mount_point);
    return 0;
}

void stop_users_vfs() {
    if (vfs_pid > 0) {
        kill(vfs_pid, SIGTERM);
        waitpid(vfs_pid, NULL, 0);
        vfs_pid = -1;
        printf("Vfs остановлен\n");
    }
}
