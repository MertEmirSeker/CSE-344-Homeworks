#ifndef MANAGER_H
#define MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include "200104004085_buffer.h"

typedef struct
{
    buffer_str *buffer;
    const char *src_dir;
    const char *dst_dir;
} manager_args;

void *manager(void *arg);
void process_directory(const char *src_dir, const char *dst_dir, buffer_str *buffer);
void copy_file(const char *src_path, const char *dst_path, buffer_str *buffer, mode_t mode);
void create_directory_if_not_exists(const char *dir_path);

#endif
