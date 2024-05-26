#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

typedef struct {
    int src_fd;
    int dst_fd;
} file_descriptor;

typedef struct {
    file_descriptor *fds;
    int size;
    int count;
    int in;
    int out;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} buffer_str;

extern volatile sig_atomic_t done;
extern volatile sig_atomic_t interrupted;
extern pthread_t *workers;
extern int num_workers;
extern pthread_t manager_thread;
extern buffer_str buffer;

// Statistics
extern int num_regular_files;
extern int num_fifo_files;
extern int num_directories;
extern size_t total_bytes_copied;

void handle_signal(int sig);
void buffer_init(buffer_str *buffer, int size);
void buffer_destroy(buffer_str *buffer);
void buffer_add(buffer_str *buffer, file_descriptor fd);
file_descriptor buffer_remove(buffer_str *buffer);
void buffer_set_done(buffer_str *buffer);
void cancel_threads();

#endif

