#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "200104004085_buffer.h"

// External global variable declarations
extern volatile sig_atomic_t done; // Indicates when to stop processing
extern size_t total_bytes_copied;  // Total bytes copied by all threads

void *worker(void *arg)
{
    buffer_str *buffer = (buffer_str *)arg; // Cast argument to buffer_str pointer
    int buffer_size = buffer->size;         // Get buffer size from the argument
    char *buf = malloc(buffer_size);        // Allocate memory for the buffer
    if (!buf)
    {
        perror("Failed to allocate buffer"); // Print error if allocation fails
        return NULL;
    }

    ssize_t bytes_read, bytes_written; // Variables to store bytes read and written
    size_t total_bytes = 0;            // Total bytes copied by this thread

    while (!done)
    {                                               // Check the 'done' variable
        file_descriptor fd = buffer_remove(buffer); // Get file descriptors from buffer
        if (fd.src_fd == -1 && fd.dst_fd == -1)
        { // Check for termination condition
            break;
        }

        // Read from source and write to destination
        while ((bytes_read = read(fd.src_fd, buf, buffer_size)) > 0)
        {
            char *write_ptr = buf;               // Pointer to buffer
            ssize_t bytes_to_write = bytes_read; // Number of bytes to write

            while (bytes_to_write > 0)
            {                                                                // While there are bytes to write
                bytes_written = write(fd.dst_fd, write_ptr, bytes_to_write); // Write to destination
                if (bytes_written == -1)
                {
                    if (errno == EINTR)
                    {             // Check if write was interrupted by a signal
                        continue; // Retry writing
                    }
                    perror("Failed to write to destination file"); // Print error if write fails
                    close(fd.src_fd);                              // Close source file descriptor
                    close(fd.dst_fd);                              // Close destination file descriptor
                    free(buf);                                     // Free allocated buffer
                    return NULL;
                }
                bytes_to_write -= bytes_written; // Decrease bytes to write by the number of bytes written
                write_ptr += bytes_written;      // Move write pointer forward
            }
            total_bytes += bytes_read; // Increase total bytes copied by bytes read
        }

        if (bytes_read == -1 && errno != EINTR)
        {                                              // Check if read failed and was not interrupted by a signal
            perror("Failed to read from source file"); // Print error if read fails
        }

        close(fd.src_fd);                                                  // Close source file descriptor
        close(fd.dst_fd);                                                  // Close destination file descriptor
        printf("Copied file from fd %d to fd %d\n", fd.src_fd, fd.dst_fd); // Print message
    }

    pthread_mutex_lock(&buffer->mutex);   // Lock the mutex to update global total bytes copied
    total_bytes_copied += total_bytes;    // Update global total bytes copied
    pthread_mutex_unlock(&buffer->mutex); // Unlock the mutex

    free(buf); // Free allocated buffer
    return NULL;
}
