#ifndef VFS_H
#define VFS_H

#include <stddef.h>

typedef struct user_info {
    char *name;
    char *path;
    char *content;
    size_t content_len;

    int uid;
    char* home;
    char* shell;
} user_info;

extern user_info users_list[];
extern int users_count;

struct user_info* find_user_by_path(const char* path);
void free_users_list(void);


int start_users_vfs(const char *mount_point);
void stop_users_vfs(void);

#endif