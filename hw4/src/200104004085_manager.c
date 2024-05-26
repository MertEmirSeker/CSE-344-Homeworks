#include "200104004085_manager.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

// External global variable declarations
extern int num_regular_files; // Counter for regular files
extern int num_fifo_files;    // Counter for FIFO files
extern int num_directories;   // Counter for directories

// Function to create a destination directory if it does not exist
void create_directory_if_not_exists(const char *dir_path)
{
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) // Check if directory exists
    {
        // Create the directory if it does not exist
        if (mkdir(dir_path, 0700) == -1)
        {
            perror("Failed to create directory"); // Print error if creation fails
            exit(EXIT_FAILURE);
        }
    }
}

// Function to ensure the parent directories exist for the given file path
void ensure_parent_directories_exist(const char *file_path)
{
    char *dir_path = strdup(file_path);        // Duplicate the file path
    char *last_slash = strrchr(dir_path, '/'); // Find the last slash
    if (last_slash)
    {
        *last_slash = '\0';                       // Terminate the string at the last slash
        create_directory_if_not_exists(dir_path); // Create the directory if it does not exist
    }
    free(dir_path); // Free the duplicated string
}

// Function to copy a file from source to destination
void copy_file(const char *src_path, const char *dst_path, buffer_str *buffer, mode_t mode)
{
    ensure_parent_directories_exist(dst_path); // Ensure parent directories exist

    if (S_ISFIFO(mode)) // Check if the file is a FIFO
    {
        if (mkfifo(dst_path, mode) == -1) // Create the FIFO
        {
            perror("Failed to create FIFO file"); // Print error if creation fails
            return;
        }
        num_fifo_files++; // Increment FIFO file counter
    }
    else // Regular file
    {
        int src_fd = open(src_path, O_RDONLY); // Open source file
        if (src_fd == -1)
        {
            perror("Failed to open source file"); // Print error if opening fails
            return;
        }

        int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, mode); // Open destination file
        if (dst_fd == -1)
        {
            perror("Failed to open destination file"); // Print error if opening fails
            close(src_fd);                             // Close source file descriptor
            return;
        }

        file_descriptor fd = {src_fd, dst_fd}; // Create file descriptor structure
        buffer_add(buffer, fd);                // Add file descriptor to buffer

        num_regular_files++; // Increment regular file counter
    }
}

// Function to process a directory
void process_directory(const char *src_dir, const char *dst_dir, buffer_str *buffer)
{
    create_directory_if_not_exists(dst_dir); // Ensure destination directory exists

    DIR *src = opendir(src_dir); // Open source directory
    if (!src)
    {
        perror("Failed to open source directory"); // Print error if opening fails
        buffer_set_done(buffer);                   // Set buffer as done
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(src)) != NULL && !done) // Read directory entries, check 'done' variable
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) // Skip . and ..
            continue;

        char src_path[PATH_MAX];                                       // Source path buffer
        char dst_path[PATH_MAX];                                       // Destination path buffer
        snprintf(src_path, PATH_MAX, "%s/%s", src_dir, entry->d_name); // Construct source path
        snprintf(dst_path, PATH_MAX, "%s/%s", dst_dir, entry->d_name); // Construct destination path

        struct stat st;
        if (lstat(src_path, &st) == -1) // Get file status
        {
            perror("Failed to lstat source file"); // Print error if lstat fails
            continue;
        }

        if (S_ISDIR(st.st_mode)) // Check if entry is a directory
        {
            process_directory(src_path, dst_path, buffer); // Recursively process directory
            num_directories++;                             // Increment directory counter
        }
        else if (S_ISFIFO(st.st_mode)) // Check if entry is a FIFO
        {
            copy_file(src_path, dst_path, buffer, st.st_mode); // Copy FIFO file
        }
        else if (S_ISREG(st.st_mode)) // Check if entry is a regular file
        {
            copy_file(src_path, dst_path, buffer, st.st_mode); // Copy regular file
        }
    }

    closedir(src); // Close source directory
}

// Manager function to handle copying process
void *manager(void *arg)
{
    manager_args *args = (manager_args *)arg; // Cast argument to manager_args pointer
    buffer_str *buffer = args->buffer;        // Get buffer from arguments
    const char *src_path = args->src_dir;     // Get source path from arguments
    const char *dst_path = args->dst_dir;     // Get destination path from arguments

    struct stat st;
    if (lstat(src_path, &st) == -1) // Get status of source path
    {
        perror("Failed to lstat source path"); // Print error if lstat fails
        buffer_set_done(buffer);               // Set buffer as done
        return NULL;
    }

    if (S_ISDIR(st.st_mode)) // Check if source path is a directory
    {
        // Create the root destination directory with the same name as the source directory
        char root_dst_dir[PATH_MAX];
        snprintf(root_dst_dir, PATH_MAX, "%s/%s", dst_path, strrchr(src_path, '/') + 1); // Construct root destination directory path
        create_directory_if_not_exists(root_dst_dir);                                    // Ensure root destination directory exists
        process_directory(src_path, root_dst_dir, buffer);                               // Process the source directory
    }
    else if (S_ISFIFO(st.st_mode)) // Check if source path is a FIFO
    {
        char dst_file[PATH_MAX];
        snprintf(dst_file, PATH_MAX, "%s/%s", dst_path, strrchr(src_path, '/') + 1); // Construct destination file path
        copy_file(src_path, dst_file, buffer, st.st_mode);                           // Copy FIFO file
    }
    else if (S_ISREG(st.st_mode)) // Check if source path is a regular file
    {
        char dst_file[PATH_MAX];
        snprintf(dst_file, PATH_MAX, "%s/%s", dst_path, strrchr(src_path, '/') + 1); // Construct destination file path
        copy_file(src_path, dst_file, buffer, st.st_mode);                           // Copy regular file
    }
    else
    {
        fprintf(stderr, "Unsupported file type\n"); // Print error for unsupported file type
        buffer_set_done(buffer);                    // Set buffer as done
        return NULL;
    }

    buffer_set_done(buffer); // Set buffer as done
    return NULL;
}
