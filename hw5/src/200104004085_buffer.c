#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include "200104004085_buffer.h"

// External global variable declarations
extern volatile sig_atomic_t done;

// Initialize the buffer with the given size
void buffer_init(buffer_str *buffer, int size)
{
    buffer->size = size;                                  // Set the buffer size
    buffer->count = 0;                                    // Initialize the count of items in the buffer
    buffer->done = 0;                                     // Initialize the done flag
    buffer->in = 0;                                       // Initialize the input index
    buffer->out = 0;                                      // Initialize the output index
    buffer->fds = malloc(size * sizeof(file_descriptor)); // Allocate memory for the file descriptors
    if (!buffer->fds)
    {
        perror("Failed to allocate buffer memory"); // Print error message if memory allocation fails
        exit(EXIT_FAILURE);                         // Exit the program
    }
    pthread_mutex_init(&buffer->mutex, NULL);                      // Initialize the mutex
    pthread_cond_init(&buffer->not_full, NULL);                    // Initialize the condition variable for not full
    pthread_cond_init(&buffer->not_empty, NULL);                   // Initialize the condition variable for not empty
    pthread_barrier_init(&buffer->barrier, NULL, num_workers + 1); // +1 for the manager thread
}

// Destroy the buffer and free its resources
void buffer_destroy(buffer_str *buffer)
{
    pthread_mutex_lock(&buffer->mutex); // Lock the mutex
    for (int i = 0; i < buffer->count; i++)
    {
        int idx = (buffer->out + i) % buffer->size; // Calculate the index
        if (buffer->fds[idx].src_fd != -1)
        {
            close(buffer->fds[idx].src_fd); // Close the source file descriptor
        }
        if (buffer->fds[idx].dst_fd != -1)
        {
            close(buffer->fds[idx].dst_fd); // Close the destination file descriptor
        }
    }
    pthread_mutex_unlock(&buffer->mutex); // Unlock the mutex

    free(buffer->fds);                        // Free the memory allocated for the file descriptors
    pthread_mutex_destroy(&buffer->mutex);    // Destroy the mutex
    pthread_cond_destroy(&buffer->not_full);  // Destroy the condition variable for not full
    pthread_cond_destroy(&buffer->not_empty); // Destroy the condition variable for not empty
    pthread_barrier_destroy(&buffer->barrier);
}

// Add a file descriptor to the buffer
void buffer_add(buffer_str *buffer, file_descriptor fd)
{
    pthread_mutex_lock(&buffer->mutex);            // Lock the mutex
    while (buffer->count == buffer->size && !done) // Wait if the buffer is full and not done
    {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex); // Wait for the not full condition
    }
    if (done) // If done, unlock the mutex and return
    {
        pthread_mutex_unlock(&buffer->mutex);
        return;
    }
    buffer->fds[buffer->in] = fd;                 // Add the file descriptor to the buffer
    buffer->in = (buffer->in + 1) % buffer->size; // Update the input index
    buffer->count++;                              // Increase the count of items in the buffer
    pthread_cond_signal(&buffer->not_empty);      // Signal the not empty condition
    pthread_mutex_unlock(&buffer->mutex);         // Unlock the mutex
}

// Remove a file descriptor from the buffer
file_descriptor buffer_remove(buffer_str *buffer)
{
    pthread_mutex_lock(&buffer->mutex);                  // Lock the mutex
    while (buffer->count == 0 && !buffer->done && !done) // Wait if the buffer is empty and not done
    {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex); // Wait for the not empty condition
    }
    if (buffer->count == 0 && (buffer->done || done)) // If the buffer is empty and done
    {
        pthread_mutex_unlock(&buffer->mutex); // Unlock the mutex
        return (file_descriptor){-1, -1};     // Return an invalid file descriptor
    }
    file_descriptor fd = buffer->fds[buffer->out];  // Get the file descriptor from the buffer
    buffer->out = (buffer->out + 1) % buffer->size; // Update the output index
    buffer->count--;                                // Decrease the count of items in the buffer
    pthread_cond_signal(&buffer->not_full);         // Signal the not full condition
    pthread_mutex_unlock(&buffer->mutex);           // Unlock the mutex
    return fd;                                      // Return the file descriptor
}

// Set the done flag of the buffer
void buffer_set_done(buffer_str *buffer)
{
    pthread_mutex_lock(&buffer->mutex);         // Lock the mutex
    buffer->done = 1;                           // Set the done flag
    pthread_cond_broadcast(&buffer->not_empty); // Broadcast the not empty condition
    pthread_cond_broadcast(&buffer->not_full);  // Broadcast the not full condition
    pthread_mutex_unlock(&buffer->mutex);       // Unlock the mutex
}