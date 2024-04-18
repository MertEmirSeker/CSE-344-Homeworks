#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"
#define COMMAND_SIZE 10

int child_count = 0;
int child1_result = 0;
int child2_result = 1;

// Signal handler for child process termination
void signal_handler(int sig)
{
    pid_t pid;
    int status;
    // Handle multiple children exiting
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);
            printf("Child with PID %ld exited with status %d\n", (long)pid, exit_status);
            child_count++;
            // Assign results based on child PID; might need a better method in real scenarios
            if (pid % 2 == 0)
            {
                child1_result = exit_status;
            }
            else
            {
                child2_result = exit_status;
            }
        }
    }
}

// Function to wait for all children to finish
void proceeding()
{
    while (child_count < 2)
    {
        printf("Proceeding...\n");
        sleep(2);
    }
    printf("All child processes have completed.\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s array_size\n", argv[0]);
        return EXIT_FAILURE;
    }

    int array_size = atoi(argv[1]);

    printf("Welcome to the hw2!!!\n");
    printf("\n");

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    // Set up signal handler for child process termination
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    int *numbers = (int *)malloc(array_size * sizeof(int));
    srand(time(NULL));

    // Generate random numbers
    for (int i = 0; i < array_size; i++)
    {
        numbers[i] = rand() % 10;
    }

    // Print generated numbers
    printf("Random generated numbers: ");
    for (int i = 0; i < array_size; i++)
    {
        printf("%d ", numbers[i]);
    }

    printf("\n");

    // Create FIFOs for inter-process communication
    if (mkfifo(FIFO1, 0666) == -1 || mkfifo(FIFO2, 0666) == -1)
    {
        perror("Failed to create FIFOs");
        exit(EXIT_FAILURE);
    }

    // Fork first child
    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        printf("Waiting for 10s in child1...\n");
        sleep(10);
        int fd1 = open(FIFO1, O_RDONLY);
        int num;
        while (read(fd1, &num, sizeof(num)) > 0)
        {
            child1_result += num;
        }
        close(fd1);

        printf("Child 1 summation: %d\n", child1_result);

        // Writing sum to fifo2
        int fd2 = open(FIFO2, O_WRONLY);
        write(fd2, &child1_result, sizeof(child1_result));
        close(fd2);
        exit(child1_result);
    }

    // Fork second child
    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        printf("Waiting for 10s in child2...\n");

        int fd2 = open(FIFO2, O_RDONLY);
        if (fd2 == -1)
        {
            perror("Failed to open FIFO2 for reading in child 2");
            exit(EXIT_FAILURE);
        }
        sleep(10);
        int total_sum;
        // Read the total sum
        if (read(fd2, &total_sum, sizeof(total_sum)) <= 0)
        {
            perror("Failed to read total sum from FIFO2 in child 2");
            exit(EXIT_FAILURE);
        }

        printf("Read sum from Child 1: %d\n", total_sum);

        char command[COMMAND_SIZE];
        int num;
        int count = 0;

        // Read numbers from FIFO2
        while (count < array_size && read(fd2, &num, sizeof(num)) > 0)
        {
            numbers[count++] = num;
        }

        // Read the command
        int bytes_read = 0, total_bytes = 0;
        while (total_bytes < COMMAND_SIZE - 1 && (bytes_read = read(fd2, command + total_bytes, COMMAND_SIZE - 1 - total_bytes)) > 0)
        {
            total_bytes += bytes_read;
        }
        command[total_bytes] = '\0'; // Null terminate for safety

        if (strcmp(command, "multiply") == 0)
        {
            for (int i = 0; i < count; i++)
            {
                child2_result *= numbers[i];
            }
        }

        printf("Command: %s\n", command);
        printf("Child 2 multiplication: %d\n", child2_result);
        printf("Final result (child1 + child2): %d\n", total_sum + child2_result);
        close(fd2);
        exit(child2_result);
    }

    // Parent process sends random numbers to FIFOs
    int fd1 = open(FIFO1, O_WRONLY);
    int fd2 = open(FIFO2, O_WRONLY);
    if (fd1 == -1 || fd2 == -1)
    {
        perror("Failed to open FIFOs for writing");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < array_size; i++)
    {
        write(fd1, &numbers[i], sizeof(numbers[i]));
    }

    close(fd1);
    sleep(10);
    for (int i = 0; i < array_size; i++)
    {
        write(fd2, &numbers[i], sizeof(numbers[i]));
    }

    const char *command = "multiply";
    write(fd2, command, strlen(command) + 1); // Include null terminator for proper string comparison

    proceeding();
    close(fd2);
    // Cleanup FIFOs
    unlink(FIFO1);
    unlink(FIFO2);

    free(numbers);

    printf("\nGoodbye!!!\n");

    return 0;
}