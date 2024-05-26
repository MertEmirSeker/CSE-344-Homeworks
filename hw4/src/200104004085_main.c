#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include "200104004085_manager.h"
#include "200104004085_worker.h"
#include "200104004085_buffer.h"

// Global variables
volatile sig_atomic_t done = 0;
volatile sig_atomic_t interrupted = 0;
pthread_t *workers = NULL;
int num_workers = 0;
pthread_t manager_thread;
buffer_str buffer;
int num_regular_files = 0;
int num_fifo_files = 0;
int num_directories = 0;
size_t total_bytes_copied = 0;

// Function prototypes
void print_usage(const char *prog_name);
void print_statistics(int num_workers, int buffer_size, double elapsed);

// Signal handler for SIGINT
void handle_signal(int sig)
{
    if (sig == SIGINT)
    {
        printf("Ctrl-C signal received.\nCopying stopped.\n");
        interrupted = 1;
        done = 1;
        buffer_set_done(&buffer);
    }
}

// Main function
int main(int argc, char *argv[])
{
    // Check command line arguments
    if (argc != 5)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    int buffer_size = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    const char *src_dir = argv[3];
    const char *dst_dir = argv[4];

    // Check command line arguments
    if (buffer_size <= 0 || num_workers <= 0)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Welcome to the MVCp directory copying system!!!\n");

    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    create_directory_if_not_exists(dst_dir);

    // Measure execution time
    struct timeval start, end;
    gettimeofday(&start, NULL);

    // Initialize buffer
    buffer_init(&buffer, buffer_size);

    printf("Files are copying...\n");

    // Start manager thread
    manager_args manager_args = {&buffer, src_dir, dst_dir};
    pthread_create(&manager_thread, NULL, manager, (void *)&manager_args);

    // Start worker threads
    workers = malloc(num_workers * sizeof(pthread_t));
    if (!workers)
    {
        perror("Failed to allocate memory for worker threads");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_workers; i++)
    {
        pthread_create(&workers[i], NULL, worker, (void *)&buffer);
    }

    // Wait for manager to finish
    pthread_join(manager_thread, NULL);

    // Wait for all workers to finish
    for (int i = 0; i < num_workers; i++)
    {
        pthread_join(workers[i], NULL);
    }

    // Destroy buffer
    buffer_destroy(&buffer);
    free(workers);

    // Measure and print execution time
    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + ((end.tv_usec - start.tv_usec) / 1000000.0);
    print_statistics(num_workers, buffer_size, elapsed);

    printf("Files are copy operation completed successfully.\nGoodbye!!!\n");

    return 0;
}

// Function to print usage message
void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <src_dir> <dst_dir>\n", prog_name);
}

// Function to print statistics
void print_statistics(int num_workers, int buffer_size, double elapsed)
{
    long milliseconds = (long)(elapsed * 1000);
    long minutes = milliseconds / 60000;
    long seconds = (milliseconds % 60000) / 1000;
    milliseconds = milliseconds % 1000;

    printf("\n--------------- ");
    printf("STATISTICS");
    printf(" ---------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of Regular Files: %d\n", num_regular_files);
    printf("Number of FIFO Files: %d\n", num_fifo_files);
    printf("Number of Directories: %d\n", num_directories);
    printf("TOTAL BYTES COPIED: %zu\n", total_bytes_copied);
    printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.milli)\n", minutes, seconds, milliseconds);
}