#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <ctype.h>
#include <signal.h>

#define SERVER_FIFO "/tmp/fifo"
#define BUFFER_SIZE 4096

// Union for semaphore initialization
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void error_exit(char *error_message);
void help(char *response, char *second_help);
void list();
void setup_server(char *server_fifo, char *dir_name);
void arch_server(char *response, char *dir_name, char *file_name);
void signal_handler(int sig);
void writeT(char *second_buffer);
void download(char *second_buffer);
void upload(char *second_buffer);
void setup_signal_handler();

int number_of_clients;
int pid_array[3];
char server_fifo[100];

int main(int argc, char *argv[])
{
    setup_signal_handler();

    if (argc < 3)
        error_exit("Unsufficient argc error!!!\n");

    number_of_clients = atoi(argv[2]); // Number of allowed clients
    char *dir_name = argv[1];          // Directory for storing server files

    char log_file[100], buffer[1024], second_buffer[1024], pid[2048], file_list[4096];

    int fds[2], child, current = 0;

    struct sembuf sembuf_wait = {0, -1, 0};
    struct sembuf sembuf_signal = {0, 1, 0};
    
    setup_server(server_fifo, dir_name);
    int fd = open(server_fifo, 0666);

    while (1)
    {
        // Clear the buffer before reading new data
        memset(buffer, 0, sizeof(buffer));
        if (read(fd, buffer, sizeof(buffer)) > 0)
        {
            // Extract PID from the buffer
            sprintf(pid, "%s", buffer);
            strtok(pid, " ");

            // Shift the buffer to the left to isolate the command
            memmove(buffer, &buffer[5], strlen(buffer) - 5 + 1);
            char *pid_token = strtok(buffer, " ");
            int ppid = atoi(pid_token);
            char *connect = strtok(NULL, " ");

            int client_assigned = 0;
            while (!client_assigned)
            {
                for (int i = 0; i < number_of_clients; i++)
                {
                    // Check if a slot is free in the pid array (if the process is no longer active)
                    if (kill(pid_array[i], 0) != 0)
                        pid_array[i] = 0;

                    // Assign the new client PID to the first free slot
                    if (pid_array[i] == 0)
                    {
                        pid_array[i] = ppid;
                        current = i;
                        client_assigned = 1;
                        break;
                    }
                }

                if (!client_assigned)
                {
                    if (strncmp(connect, "connect", 7) == 0 || strncmp(connect, "tryConnect", 10) == 0)
                    {
                        // If trying to connect and no slots are available, sleep for 1 second and try again
                        sleep(1);
                    }
                    else
                    {
                        // If not trying to connect explicitly, print queue full message and break
                        printf("Connection request PID %d... Queue is FULL\n", ppid);
                        break;
                    }
                }
            }

            if (!client_assigned)
                continue;

            // Fork a new process to handle the client
            child = fork();

            if (child == 0)
            {
                // Get semaphore IDs for synchronization
                int semid = semget(ppid, 1, 0666);
                if (semid < 0)
                    error_exit("semget error!!!\n");
                int semid2 = semget(ppid + 1, 1, 0666);
                if (semid2 < 0)
                    error_exit("semget error!!!\n");

                int client_fd;

                // Create a FIFO special file for client-server communication
                if (pid_array[current] == ppid)
                {
                    if (mkfifo(pid, 0666) == -1)
                        error_exit("mkfifo error!!!\n");
                    client_fd = open(pid, 0666);
                    if (client_fd < 0)
                        error_exit("open error!!!\n");
                }

                // Post on semaphore to signal readiness
                if (semop(semid, &sembuf_signal, 1) < 0)
                    error_exit("semop error!!!\n");

                // Continue only if the current slot is still assigned to this client
                if (pid_array[current] != ppid)
                {
                    continue;
                }

                // Open or create a log file for this client
                sprintf(log_file, "%s/%d", dir_name, ppid);
                int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);

                printf("Client PID %d connected as client%d\n", ppid, current + 1);

                while (1)
                {
                    // Wait on semaphore to synchronize access
                    if (semop(semid2, &sembuf_wait, 1) == -1)
                        error_exit("semop error!!!\n");

                    // Clear the buffer before reading new data
                    memset(second_buffer, 0, sizeof(second_buffer));
                    if (read(client_fd, second_buffer, sizeof(second_buffer)) > 0)
                    {
                        // Log the received command to a file
                        char log[100];
                        write(fd, second_buffer, strlen(second_buffer));
                        write(fd, "\n", 1);
                        pipe(fds);

                        int new_child = fork();

                        if (new_child == 0)
                        {
                            // Redirect standard output to the pipe
                            dup2(fds[1], STDOUT_FILENO);

                            // Check which command is being issued and execute accordingly
                            if (strncmp(second_buffer, "list", 4) == 0)
                            {
                                list();
                            }

                            else if (strncmp(second_buffer, "readF", 5) == 0)
                            {
                                // Parse the command to get filename and line number
                                strtok(second_buffer, " ");
                                char *filename = strtok(NULL, " ");
                                char *lineNumberStr = strtok(NULL, "\n");

                                if (filename == NULL)
                                {
                                    printf("Filename is required.\n");
                                    return 0;
                                }

                                if (lineNumberStr != NULL)
                                {
                                    int lineNumber = atoi(lineNumberStr);
                                    if (lineNumber > 0)
                                    {
                                        // Execute command to print specific line of the file
                                        char command[256];
                                        snprintf(command, sizeof(command), "sed -n '%dp' %s", lineNumber, filename);
                                        system(command);
                                    }
                                    else
                                    {
                                        printf("Invalid line number.\n");
                                    }
                                }
                                else
                                {
                                    // Print the entire file if no line number is specified
                                    char command[256];
                                    snprintf(command, sizeof(command), "cat %s", filename);
                                    system(command);
                                }
                            }
                            else if (strncmp(second_buffer, "writeT", 6) == 0)
                                writeT(second_buffer);

                            else if (strncmp(second_buffer, "download", 8) == 0)
                                download(second_buffer);

                            else if (strncmp(second_buffer, "upload", 6) == 0)
                                upload(second_buffer);

                            exit(1);
                        }
                        else
                        {
                            // Wait for the child process to complete
                            waitpid(new_child, NULL, 0);
                            char response[10240];
                            memset(response, 0, sizeof(response));

                            if (strncmp(second_buffer, "writeT", 6) == 0 || strncmp(second_buffer, "download", 8) == 0 || strncmp(second_buffer, "upload", 6) == 0)
                            {
                                strcpy(response, "Task completed successfully.\n");
                            }
                            else if (strncmp(second_buffer, "killServer", 10) == 0)
                            {
                                printf("Kill signal from client%d... terminating...\n", current + 1);
                                printf("Goodbye...\n");
                                fflush(stdout);
                                unlink(server_fifo);
                                // Send termination signal to all child processes
                                for (int i = 0; i < 3; i++)
                                {
                                    char client_fifo[256];
                                    sprintf(client_fifo, "/tmp/%d", pid_array[i]);
                                    unlink(client_fifo);
                                    kill(pid_array[i], SIGTERM);
                                }
                            }
                            else if (strncmp(second_buffer, "quit", 4) == 0)
                            {
                                strcpy(response, "Sending write request to server log file\nwaiting for log file...\n");
                                printf("Sending write request to server log file\nwaiting for log file...\n");
                                printf("Client PID %d (client%d) disconnected.\n", ppid, current + 1);
                            }
                            else if (strncmp(second_buffer, "help", 4) == 0)
                            {
                                strtok(second_buffer, " ");
                                char *second_help = strtok(NULL, "\n");
                                help(response, second_help);
                            }
                            else if (strncmp(second_buffer, "archServer", 10) == 0)
                            {
                                strtok(second_buffer, " ");
                                char *file_name = strtok(NULL, "\n");
                                arch_server(response, dir_name, file_name);
                            }

                            else
                                // Read the output from the child process through the pipe
                                read(fds[0], response, sizeof(response));

                            // Send the response back to the client
                            write(client_fd, response, sizeof(response));
                            if (semop(semid2, &sembuf_signal, 1) < 0)
                                error_exit("semop error!!!\n");
                        }
                    }
                }
            }
        }
    }

    return 0;
}

void error_exit(char *error_message)
{
    perror(error_message);
    exit(EXIT_FAILURE);
}

void list()
{
    char *args[] = {"ls", NULL};
    execv("/bin/ls", args); // Execute the 'ls' command, replacing the current
}

void arch_server(char *response, char *dir_name, char *file_name)
{
    // Trim trailing spaces from the filename
    size_t file_name_length = strlen(file_name);
    while (file_name_length > 0 && isspace(file_name[file_name_length - 1]))
    {
        file_name[--file_name_length] = '\0';
    }

    // Check if filename is provided, if not, send error response
    if (file_name == NULL)
        strcpy(response, "Error: No filename provided.\n");

    else
    {
        // Fork a new process to handle the archiving
        int archive_child = fork();
        if (archive_child == 0)
        {
            // Child process: Executes the tar command to archive the server's content
            printf("Archiving the current contents of the server...\n");
            char tarCommand[1024];
            char parent_dir[1024];

            // Construct the path to the parent directory and the tar command
            sprintf(parent_dir, "%s/..", dir_name);
            sprintf(tarCommand, "%s/%s.tar", parent_dir, file_name);
            printf("%s\n", dir_name);
            printf("%s\n", file_name);

            // Execute the tar command to create an archive
            execlp("tar", "tar", "cf", tarCommand, "-C", parent_dir, ".", NULL);
            perror("Failed to create archive!!!");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Parent process waits for the child to complete
            int status;
            waitpid(archive_child, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            {
                // If child process exits successfully, update response to indicate success
                printf("Calling tar utility .. child PID %d\n", archive_child);
                printf("Child process completed successfully.\n");
                printf("Copying the archive file..\n");
                sprintf(response, "SUCCESS: Server side files are archived in \"%s.tar\"\n ", file_name);
            }
            else
                // If child process fails, update response to indicate error
                strcpy(response, "ERROR in archiving process.\n");
        }
    }
}

void setup_server(char *server_fifo, char *dir_name)
{
    mkdir(dir_name, 0777);
    sprintf(server_fifo, "/tmp/%d", getpid());
    mkfifo(server_fifo, 0666);
    printf("Server started PID %d\n", getpid());
    printf("Server waiting for clients...\n");
}

void help(char *response, char *second_help)
{
    if (second_help == NULL)
        strcpy(response, "\nAvailable commands are:\nhelp, list, readF, writeT, upload, download, archServer, killServer, quit\n");

    else if (strncmp(second_help, "list", 4) == 0)
        strcpy(response, "\nlist\nDisplay the list of files in servers directory.\n");

    else if (strncmp(second_help, "readF", 5) == 0)
        strcpy(response, "\nreadF <file> <line #>\n\tRequests to display the # line of the <file>, if no line number is given the whole contents of the file is requested. (and displayed on the client side)\n");

    else if (strncmp(second_help, "writeT", 6) == 0)
        strcpy(response, "\nwriteT <file> <line #> <string>\nRequest to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time\n");

    else if (strncmp(second_help, "upload", 6) == 0)
        strcpy(response, "\nupload <file>\nUploads the file from the current working directory of client to the servers directory.\n");

    else if (strncmp(second_help, "download", 8) == 0)
        strcpy(response, "\ndownload <file>\nRequest to receive <file> from servers directory to client side\n");

    else if (strncmp(second_help, "archServer", 10) == 0)
        strcpy(response, "\narchServer <fileName>.tar\nUsing fork, exec and tar utilities create a child process that will collect all the files currently available on the the Server side and store them in the <filename>.tar archive\n");

    else if (strncmp(second_help, "killServer", 10) == 0)
        strcpy(response, "\nkillServer\nSends a kill request to the Server.\n");

    else if (strncmp(second_help, "quit", 4) == 0)
        strcpy(response, "\nquit\nSend write request to Server side log file and quits.\n");

    else
        strcpy(response, "\nInvalid command. Please use 'help' to see the list of available commands.\n");
}

void signal_handler(int sig)
{
    printf("Ctrl-C received, shutting down...\n");

    unlink(server_fifo);
    for (int i = 0; i < number_of_clients; i++)
    {
        if (pid_array[i] != 0)
        {
            char client_fifo[256];
            sprintf(client_fifo, "/tmp/%d", pid_array[i]);
            unlink(client_fifo);
            kill(pid_array[i], SIGTERM);
        }
    }

    exit(0);
}
void setup_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);       // Initialize the signal set to empty, then add signals to block.
    sigaddset(&sa.sa_mask, SIGINT); // Block SIGINT during the execution of the handler.
    sa.sa_flags = 0;                // Use default settings.

    sigaction(SIGINT, &sa, NULL);
}

void writeT(char *second_buffer)
{
    char *args[] = {"sed", "-i", "", "", NULL};
    strtok(second_buffer, " ");
    args[3] = strtok(NULL, " ");
    char *last_arg = strtok(NULL, "\n");
    char *copy = strdup(last_arg);
    char *number_str = strtok(last_arg, " ");
    int number = atoi(number_str);

    if (number == 0 && number_str != "0")
    {
        // Append text if no valid line number is provided
        args[2] = malloc((4 + strlen(copy)) * sizeof(char));
        sprintf(args[2], "$ a\\%s", copy);
    }
    else
    {
        // Replace text at the specified line number
        args[2] = malloc((strlen(number_str) + strlen(copy)) * sizeof(char));
        copy[0] = 'c';
        copy[1] = '\\';
        sprintf(args[2], "%s%s", number_str, copy); // Replacement command for 'sed'
    }

    execv("/bin/sed", args); // Execute 'sed' with the constructed command
}

void download(char *second_buffer)
{
    char *args[] = {"cp", "", "", NULL};
    strtok(second_buffer, " ");
    args[1] = strtok(NULL, " ");
    args[2] = strtok(NULL, " ");
    execv("/bin/cp", args); // Execute 'cp' to perform the download
}

void upload(char *second_buffer)
{
    char *args[] = {"cp", "", ".", NULL};
    args[1] = malloc(256 * sizeof(char)); // Allocate memory for the source path
    strtok(second_buffer, " ");
    strcpy(args[1], strcat(strtok(NULL, " "), strtok(NULL, " ")));
    execv("/bin/cp", args); // Execute 'cp' to perform the upload
}