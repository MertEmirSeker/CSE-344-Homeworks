#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <stdbool.h>
#include <string.h>

#define SERVER_FIFO "/tmp/fifo"

// Union used to set semaphore value
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Structure for semaphore operations (wait and signal)
struct sembuf sembuf_wait = {0, -1, 0};
struct sembuf sembuf_signal = {0, 1, 0};

// Array of valid commands for the server
const char *valid_commands[] = {"upload", "download", "quit", "list", "readF", "writeT", "help", "killServer", "archServer"};
const int valid_commands_count = sizeof(valid_commands) / sizeof(valid_commands[0]);

void error_exit(char *error_message);
int initialize_semaphore(int key, int value);
void semaphore_wait(int semid);
void semaphore_signal(int semid);
int open_fifo(const char *fifo_name, int flags);
void handle_client(int client_fd, int semid, int semid2);
bool is_valid_command(const char *command);

int main(int argc, char *argv[])
{
    if (argc < 3)
        error_exit("Unsufficient argc error!!!\n");

    int server_pid = atoi(argv[2]);
    char *option = argv[1];
    char server_fifo[100];
    sprintf(server_fifo, "/tmp/%d", server_pid);
    printf("Waiting for Que...\n");

    // Initialize semaphores for synchronization
    int semid = initialize_semaphore(getpid(), 0);
    int semid2 = initialize_semaphore(getpid() + 1, 0);

    int fd = open_fifo(server_fifo, 0666);

    // Send client identification to server
    char pid[100];
    sprintf(pid, "/tmp/%d %s", getpid(), argv[1]);
    write(fd, pid, strlen(pid));
    char *token = strtok(pid, " ");
    if (token != NULL)
    {
        memmove(pid, token, strlen(token) + 1); // Include null terminator
    }

    semaphore_wait(semid);

    int client_fd = open_fifo(pid, 0666);

    // Handle client requests
    handle_client(client_fd, semid, semid2);

    return 0;
}

int initialize_semaphore(int key, int value)
{
    union semun semun_val;
    semun_val.val = value;
    int semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid < 0 || semctl(semid, 0, SETVAL, semun_val) < 0)
        error_exit("Semaphore initialization failed!!!");
    
    return semid;
}

void semaphore_wait(int semid)
{
    if (semop(semid, &sembuf_wait, 1) == -1)
        error_exit("semop wait error!!!\n");
}

void semaphore_signal(int semid)
{
    if (semop(semid, &sembuf_signal, 1) == -1)
        error_exit("semop signal error!!!\n");
}

int open_fifo(const char *fifo_name, int flags)
{
    int fd = open(fifo_name, flags);
    if (fd < 0)
    {
        error_exit("open fifo error!!!\n");
    }
    return fd;
}

void handle_client(int client_fd, int semid, int semid2)
{
    char buffer[65536], line[65536], cwd[100];
    int bytes_read;

    printf("Connection established!\n");

    while (true)
    {
        memset(buffer, 0, sizeof(buffer)); // Clear buffer before use
        printf("\nEnter command: ");
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("Error reading input, please try again.\n");
            continue;
        }
        line[strlen(line) - 1] = '\0'; // Proper null termination after removing newline

        if (strcmp(line, "quit") == 0)
        {
            strncpy(buffer, line, sizeof(buffer) - 1);
    write(client_fd, buffer, strlen(buffer));
    semaphore_signal(semid2);
            break;

        }

        if (!is_valid_command(line))
        {
            printf("Invalid command. Please try a valid command.\n");
            continue;
        }

        // Prepare command for upload or download with current working directory
        if (strncmp(line, "upload", 6) == 0 || strncmp(line, "download", 8) == 0)
        {
            if (!getcwd(cwd, sizeof(cwd)))
            {
                printf("Failed to get current working directory.\n");
                continue;
            }
            snprintf(buffer, sizeof(buffer), "%s %s/", line, cwd); // Safer buffer handling with snprintf
        }
        else
        {
            strncpy(buffer, line, sizeof(buffer) - 1); // Use safer string copy
            buffer[sizeof(buffer) - 1] = '\0';         // Ensure null termination
        }

        // Communicate with server
        write(client_fd, buffer, strlen(buffer));
        semaphore_signal(semid2);
        semaphore_wait(semid2);

        // Read and display server response
        while ((bytes_read = read(client_fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
            printf("%s", buffer);

            if (bytes_read < sizeof(buffer) - 1)
                break; // Break if we read less than the buffer size, likely means we've reached the end
        }
    }
}

void error_exit(char *error_message)
{
    perror(error_message);
    exit(EXIT_FAILURE);
}

bool is_valid_command(const char *command)
{
    for (int i = 0; i < valid_commands_count; i++)
    {
        if (strncmp(command, valid_commands[i], strlen(valid_commands[i])) == 0)
            return true;
        
    }
    return false;
}
