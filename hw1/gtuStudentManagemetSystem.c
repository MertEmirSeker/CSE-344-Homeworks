#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#define MAX_SIZE 1000000
#define MAX_STUDENTS 10000

#define LOG_FILE_PATH "gtuStudentManagemetSystem.log"

int menu();
void commands();
void remove_quotes(char *string);
void sort_by_student_name_ascending(const char *filename);
void sort_by_student_name_descending(const char *filename);
void sort_by_grade_ascending(const char *filename);
void sort_by_grade_descending(const char *filename);
void log_message(const char *message);
int is_valid_grade(const char *grade);

void gtu_student_grades(const char *filename);
void add_student_grade(const char *filename, const char *name_surname, const char *grade);
void search_student(const char *filename, const char *name_surname);
void show_all(const char *filename);
void list_grades(const char *filename);
void list_some(const char *filename, int num_of_entries, int page_number);
void sort_all(const char *filename);

int main()
{
	printf("Welcome to the Student Grade Management System!!!\n");

	commands();
	menu();

	return 0;
}

void gtu_student_grades(const char *filename)
{
    // Create a new process by duplicating the calling process
    pid_t pid = fork();

    // Check if fork failed
    if (-1 == pid)
    {
        perror("Fork failed!\n");
        log_message("Fork failed in gtu_student_grades."); 
        exit(EXIT_FAILURE);
    }
	// Child process
    else if (0 == pid) 
    {
        // Open or create a file with write-only access, fail if the file already exists
        int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0644);

        // Check if opening the file failed
        if (-1 == fd)
        {
            perror("Failed to open file.\n");
            log_message("Failed to open file in gtu_student_grades."); 
            exit(EXIT_FAILURE);
        }

        close(fd);
        exit(EXIT_SUCCESS);
    }
	// Parent process
    else 
    {
        int status;
        waitpid(pid, &status, 0);	// Wait for the child process to change state

        // Check if the child process terminated normally
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            printf("File created.\n");
            log_message("File created successfully."); 
        }
        else
        {
            printf("Failed to create file.\n");
            log_message("Failed to create file in parent process of gtu_student_grades."); 
        }
    }
}



void add_student_grade(const char *filename, const char *name_surname, const char *grade)
{

	pid_t pid = fork(); // Forks the process.

	if (pid == -1) // Checks for fork failure.
	{
		perror("Fork failed");
		log_message("Fork failed in add_student_grade.");
		exit(EXIT_FAILURE);
	}
	else if (pid == 0) // Child process operations.
	{
		char temp[MAX_SIZE]; // Buffer for reading file content.
		int found = 0; // Flag indicating if the student record is found.
		ssize_t total_bytes_read = 0; // Total bytes read from the file.

		int fd = open(filename, O_RDONLY); // Opens the file for reading.
		if (fd != -1) // Checks if the file exists and is open.
		{
			// Reads the file content into temp.
			ssize_t readed_bytes;
			while ((readed_bytes = read(fd, temp + total_bytes_read, sizeof(temp) - total_bytes_read - 1)) > 0)
			{
				total_bytes_read += readed_bytes;
			}
			temp[total_bytes_read] = '\0'; // Null-terminates the string.
			close(fd); 

			// Processes each line in the temp buffer.
			char *line = strtok(temp, "\n");
			char space[MAX_SIZE] = ""; // Buffer for the new file content.
			while (line != NULL)
			{
				if (strncmp(line, name_surname, strlen(name_surname)) == 0) // Checks if the current line is the student's record.
				{
					// Updates or adds the student's grade.
					snprintf(space + strlen(space), sizeof(space) - strlen(space), "%s, %s\n", name_surname, grade);
					found = 1; // Indicates the record was found and updated.
				}
				else
				{
					// Copies other records as is.
					strncat(space, line, sizeof(space) - strlen(space) - 1);
					strcat(space, "\n");
				}
				line = strtok(NULL, "\n");
			}

			// Adds the student's grade if not found.
			if (!found)
			{
				snprintf(space + strlen(space), sizeof(space) - strlen(space), "%s, %s\n", name_surname, grade);
			}

			// Reopens the file for writing, truncating existing content.
			fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd == -1)
			{
				perror("Failed to open file for writing");
				log_message("Failed to open file for writing in add_student_grade.");
				exit(EXIT_FAILURE);
			}

			// Writes the updated content to the file.
			if (write(fd, space, strlen(space)) != (ssize_t)strlen(space))
			{
				perror("Failed to write updated content to file");
				log_message("Failed to write updated content to file in add_student_grade.");
				close(fd);
				exit(EXIT_FAILURE);
			}

			close(fd); 

			if (found)
            {
                printf("Student grade updated successfully.\n"); 
                log_message("Student grade updated successfully.");
            }
            else
            {
                printf("New student grade added successfully.\n"); 
                log_message("New student grade added successfully.");
            }
		}
		else
		{
			// Handles the case where the file does not exist or cannot be opened for reading.
			fd = open(filename, O_WRONLY | O_CREAT, 0644);
			if (fd == -1)
			{
				perror("Failed to open file for first-time writing.");
				log_message("Failed to open file for first-time writing in add_student_grade.");
				exit(EXIT_FAILURE);
			}
			char record[MAX_SIZE];
			snprintf(record, sizeof(record), "%s, %s\n", name_surname, grade);
			if (write(fd, record, strlen(record)) != (ssize_t)strlen(record))
			{
				perror("Failed to write to file.");
				log_message("Failed to write to file in add_student_grade.");
				close(fd);
				exit(EXIT_FAILURE);
			}
			close(fd);
		}
		exit(EXIT_SUCCESS);
	}
	else
	{
		// Parent process waits for the child to complete
		wait(NULL);
	}
}

void search_student(const char *filename, const char *name_surname)
{
    // Create a new process using fork().
    pid_t pid = fork();

    // Check if fork failed.
    if (-1 == pid)
    {
        perror("Fork failed!\n"); 
        log_message("Fork failed in search_student."); 
        exit(EXIT_FAILURE); 
    }
    else if (0 == pid) // Child process code block.
    {
        // Open the file in read-only mode.
        int fd = open(filename, O_RDONLY);

        // Check if opening the file failed.
        if (-1 == fd)
        {
            perror("Failed to open file.\n"); 
            log_message("Failed to open file in search_student."); 
            exit(EXIT_FAILURE); 
        }

        // Initialize variables for reading the file.
        char buffer;
        char line[MAX_SIZE];
        int i = 0, found = 0;

        // Read the file character by character.
        while (read(fd, &buffer, 1) > 0)
        {
            if ('\n' == buffer) // Check for newline character indicating the end of a line.
            {
                line[i] = '\0'; // Null-terminate the current line.
                i = 0; 

                // Check if the current line contains the name_surname string.
                if (strstr(line, name_surname) != NULL)
                {
                    printf("Student found: %s\n", line); 
                    log_message("Student found in search_student."); 
                    found = 1; 
                    break; 
                }
            }
            else
            {
                // Append character to line if within buffer limit.
                if (i < MAX_SIZE - 1)
                {
                    line[i++] = buffer;
                }
            }
        }

        // If the student was not found, print and log a message.
        if (0 == found)
        {
            printf("Student not found.\n");
            log_message("Student not found in search_student.");
        }

     
        close(fd);
        exit(EXIT_SUCCESS);
    }
    else // Parent process code block.
    {
        int status;
        waitpid(pid, &status, 0);

        // Check the exit status of the child process.
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            // If the child exited successfully, there's no additional action needed here.
        }
        else
        {
            printf("An error occurred while searching for the student.\n");
            log_message("An error occurred in parent process of search_student.");
        }
    }
}

void show_all(const char *filename)
{
    // Forks the current process to create a child process.
    pid_t pid = fork();

    // Checks if fork failed.
    if (-1 == pid)
    {
        perror("Fork failed!\n"); 
        log_message("Fork failed in show_all."); 
        exit(EXIT_FAILURE); 
    }
    else if (0 == pid) // Child process branch.
    {
        // Opens the specified file in read-only mode.
        int fd = open(filename, O_RDONLY);

        // Checks if opening the file failed.
        if (-1 == fd)
        {
            perror("Failed to open file.\n"); 
            log_message("Failed to open file in show_all."); 
            exit(EXIT_FAILURE); 
        }

        char buffer[1]; 
        int isEmpty = 1; 

        // Reads the file byte by byte.
        while (read(fd, buffer, sizeof(buffer)) > 0)
        {
            isEmpty = 0; 
            write(STDOUT_FILENO, buffer, sizeof(buffer)); 
        }

        if (isEmpty)
        {
            printf("The file is empty.\n");
            log_message("Attempted to show an empty file in show_all.");
        }

        close(fd); 
        exit(EXIT_SUCCESS); 
    }
    else // Parent process branch.
    {
        wait(NULL); // Waits for the child process to finish.
        log_message("Displayed all contents successfully in show_all.");
    }
}

void list_grades(const char *filename)
{
    // Forks the current process to create a child process.
    pid_t pid = fork();

    // Checks if fork failed.
    if (-1 == pid)
    {
        perror("Fork failed!\n"); 
        log_message("Fork failed in list_grades."); 
        exit(EXIT_FAILURE); 
    }
    else if (0 == pid) // Child process branch.
    {
        // Opens the specified file in read-only mode.
        int fd = open(filename, O_RDONLY);

        // Checks if opening the file failed.
        if (-1 == fd)
        {
            perror("Failed to open file.\n"); 
            log_message("Failed to open file in list_grades."); 
            exit(EXIT_FAILURE); 
        }

        int line_counter = 0; 
        char ch; 
        int isEmpty = 1; 

        // Reads the file character by character, stopping after five lines.
        while (read(fd, &ch, 1) > 0 && line_counter < 5)
        {
            isEmpty = 0; 
            printf("%c", ch); // Prints each character to display the line.
            if ('\n' == ch)
            {
                line_counter++; 
            }
        }

        if (isEmpty)
        {
            // If the file is empty, informs the user and logs the event.
            printf("The file is empty.\n");
            log_message("Attempted to list grades from an empty file in list_grades.");
        }

        close(fd); 
        exit(EXIT_SUCCESS); 
    }
    else // Parent process branch.
    {
        wait(NULL); // Waits for the child process to finish.
        log_message("Displayed the first 5 grades successfully in list_grades."); 
    }
}

void list_some(const char *filename, int num_of_entries, int page_number)
{
    // Creates a new process by forking the current one.
    pid_t pid = fork();

    // Checks if the fork operation failed.
    if (-1 == pid)
    {
        perror("Fork failed!\n"); 
        log_message("Fork failed in list_some."); 
        exit(EXIT_FAILURE); 
    }
    else if (0 == pid) // Child process branch.
    {
        // Opens the specified file in read-only mode.
        int fd = open(filename, O_RDONLY);

        // Checks if opening the file failed.
        if (-1 == fd)
        {
            perror("Failed to open file.\n"); 
            log_message("Failed to open file in list_some."); 
            exit(EXIT_FAILURE); 
        }

        // Calculates the start and end line numbers to be displayed based on the specified page.
        int start_line = num_of_entries * (page_number - 1) + 1;
        int end_line = start_line + num_of_entries - 1;
        int current_line = 1;
        char ch;
        int isEmpty = 1; 

        // Reads the file character by character.
        while (read(fd, &ch, 1) > 0)
        {
            isEmpty = 0; 
            
            // Checks if the current line is within the range to be displayed.
            if (current_line >= start_line && current_line <= end_line)
            {
                write(STDOUT_FILENO, &ch, 1); // Writes the character to standard output.
            }

            // Breaks out of the loop if the end of the specified range is reached.
            if (current_line > end_line)
            {
                break;
            }

            // Increments the line counter upon encountering a newline character.
            if ('\n' == ch)
                current_line++;
        }

        // Handles the case where the file is empty.
        if (isEmpty)
        {
            printf("The file is empty.\n"); 
            log_message("Attempted to list some entries from an empty file in list_some."); 
        }
        else if (current_line < start_line)
        {
            printf("The requested page is beyond the file content.\n"); 
            log_message("Attempted to access a page beyond the file content in list_some."); 
        }

        close(fd); 
        exit(EXIT_SUCCESS);
    }
    else // Parent process branch.
    {
        wait(NULL); // Waits for the child process to complete.
        log_message("Displayed requested entries successfully in list_some."); 
    }
}

void sort_all(const char *filename)
{
	printf("Sort Menu\n");
	printf("1. Sort by Name Ascending\n");
	printf("2. Sort by Name Descending\n");
	printf("3. Sort by Grade Ascending\n");
	printf("4. Sort by Grade Descending\n");
	printf("Enter choice: ");

	int choice;
	scanf("%d", &choice);

	if (1 == choice)
	{
		sort_by_student_name_ascending(filename);
	}

	else if (2 == choice)
	{
		sort_by_student_name_descending(filename);
	}

	else if (3 == choice)
	{
		sort_by_grade_ascending(filename);
	}

	else if (4 == choice)
	{
		sort_by_grade_descending(filename);
	}

	else
		printf("Invalid choice. Returning to main menu.\n");
}

// helper functions
int is_valid_grade(const char *grade)
{
	const char *grade_order[] = {"NA", "VF", "FF", "DD", "DC", "CC", "CB", "BB", "BA", "AA"};
	int n = sizeof(grade_order) / sizeof(grade_order[0]);
	for (int i = 0; i < n; i++)
	{
		if (strcmp(grade, grade_order[i]) == 0)
		{
			return 1;
		}
	}
	return 0; 
}


// menu function
int menu()
{
	char command[MAX_SIZE];
	char filename[MAX_SIZE];

	while (1)
	{
		printf("Enter command(\"q\" for quit and \"help\" for available commands): ");

		scanf("%s", command);

		if (0 == strcmp(command, "q"))
		{
			printf("Exiting program. Goodbye!!!\n");
			log_message("Exiting program");
			return 0;
		}
		else if (0 == strcmp(command, "gtuStudentGrades"))
		{
			if (scanf("%s", filename) != 1)
			{
				printf("Error: Incorrect number of arguments for gtuStudentGrades.\n");
				log_message("Error: Incorrect number of arguments for gtuStudentGrades.");
			}
			else
			{
				remove_quotes(filename);
				gtu_student_grades(filename);
			}
		}
		else if (0 == strcmp(command, "help"))
		{
			commands();
		}
		else if (0 == strcmp(command, "addStudentGrade"))
		{
			char grade[MAX_SIZE];
			char name_surname[2 * MAX_SIZE];
			if (scanf(" \"%[^\"]\" \"%[^\"]\" \"%[^\"]\"", filename, name_surname, grade) != 3)
			{
				printf("Error: Incorrect number of arguments for addStudentGrade.\n");
				log_message("Error: Incorrect number of arguments for addStudentGrade.");
			}
			else
			{
				// Notun geçerli olup olmadığını burada kontrol et
				if (!is_valid_grade(grade))
				{
					printf("Invalid grade value. Please enter one of the following grades: NA, VF, FF, DD, DC, CC, CB, BB, BA, AA.\n");
					log_message("Attempted to add an invalid grade.");
				}
				else
				{
					// Eğer not geçerliyse, add_student_grade fonksiyonunu çağır
					add_student_grade(filename, name_surname, grade);
				}
			}
		}

		else if (0 == strcmp(command, "searchStudent"))
		{
			char name_surname[2 * MAX_SIZE];
			if (scanf(" \"%[^\"]\" \"%[^\"]\"", filename, name_surname) != 2)
			{
				printf("Error: Incorrect number of arguments for searchStudent.\n");
				log_message("Error: Incorrect number of arguments for searchStudent.");
			}
			else
			{
				search_student(filename, name_surname);
			}
		}
		else if (0 == strcmp(command, "showAll"))
		{
			if (scanf("%s", filename) != 1)
			{
				printf("Error: Incorrect number of arguments for showAll.\n");
				log_message("Error: Incorrect number of arguments for showAll.");
			}
			else
			{
				remove_quotes(filename);
				show_all(filename);
			}
		}
		else if (0 == strcmp(command, "listGrades"))
		{
			if (scanf("%s", filename) != 1)
			{
				printf("Error: Incorrect number of arguments for listGrades.\n");
				log_message("Error: Incorrect number of arguments for listGrades.");
			}
			else
			{
				remove_quotes(filename);
				list_grades(filename);
			}
		}
		else if (0 == strcmp(command, "listSome"))
		{
			int number_of_entries, page_number;
			if (scanf("%s %d %d", filename, &number_of_entries, &page_number) != 3)
			{
				printf("Error: Incorrect number of arguments for listSome.\n");
				log_message("Error: Incorrect number of arguments for listSome.");
			}
			else
			{
				remove_quotes(filename);
				list_some(filename, number_of_entries, page_number);
			}
		}
		else if (0 == strcmp(command, "sortAll"))
		{
			if (scanf("%s", filename) != 1)
			{
				printf("Error: Incorrect number of arguments for sortAll.\n");
				log_message("Error: Incorrect number of arguments for sortAll.");
			}
			else
			{
				remove_quotes(filename);
				sort_all(filename);
			}
		}
		else
		{
			printf("Unknown command.\n");
			log_message("Unknown command received in menu");
		}
	}
}

// removes to quotes
void remove_quotes(char *string)
{
	int i, j = 0;
	for (i = 0; string[i] != '\0'; i++)
	{
		if (string[i] != '"')
			string[j++] = string[i];
	}

	string[j] = '\0';
}

void commands()
{
	printf("Available Commands:\n");
	printf("\n");
	printf("1. help\n");
	printf("2. gtuStudentGrades \"filename\"\n");
	printf("3. addStudentGrade \"filename\" \"Name Surname\" \"Grade\"\n");
	printf("4. searchStudent \"filename\" \"Name Surname\"\n");
	printf("5. sortAll \"filename\"\n");
	printf("6. showAll \"filename\"\n");
	printf("7. listGrades \"filename\"\n");
	printf("8. listSome \"filename\" numOfEntries pageNumber\n");
}

void log_message(const char *message)
{
    pid_t pid = fork();

    if (pid == -1)
    {
        // Fork failed.
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish the process.
    }
    else
    {
        // child process
        int fd = open(LOG_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0)
        {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }

        size_t message_length = strlen(message);

        if (write(fd, message, message_length) != message_length ||
            write(fd, "\n", 1) != 1)
        {
            perror("Failed to write to log file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        close(fd);
        exit(EXIT_SUCCESS); 
    }
}

void sort_by_student_name_ascending(const char *filename)
{
    // Forks the current process to create a child process.
    pid_t pid = fork();

    // Checks if the fork operation failed.
    if (pid == -1)
    {
        perror("Fork failed\n"); 
        log_message("Fork failed in sort_by_student_name_ascending."); 
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // Child process block
        // Opens the specified file in read-only mode.
        int fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            perror("Error opening file\n"); 
            log_message("Error opening file in sort_by_student_name_ascending."); 
            exit(EXIT_FAILURE); 
        }

        char buffer[MAX_SIZE]; 
        ssize_t readed_bytes = read(fd, buffer, sizeof(buffer) - 1); // Reads the file into the buffer.
        if (readed_bytes == -1)
        {
            perror("Error reading file\n"); 
            log_message("Error reading file in sort_by_student_name_ascending."); 
            close(fd); 
            exit(EXIT_FAILURE); 
        }
        buffer[readed_bytes] = '\0'; // Ensures null-termination of the string.
        close(fd); 

        char *lines[MAX_SIZE / 30]; 
        int lines_number = 0; 
        // Splits the buffer into lines.
        char *line = strtok(buffer, "\n");
        while (line != NULL)
        {
            lines[lines_number++] = line; 
            line = strtok(NULL, "\n"); 
        }

        // Sorts the lines using a simple bubble sort algorithm.
        for (int i = 0; i < lines_number - 1; i++)
        {
            for (int j = 0; j < lines_number - i - 1; j++)
            {
                if (strcmp(lines[j], lines[j + 1]) > 0)
                {
                    char *temp = lines[j];
                    lines[j] = lines[j + 1];
                    lines[j + 1] = temp;
                }
            }
        }

        log_message("Sorted by student name ascending successfully.");

        for (int i = 0; i < lines_number; i++)
        {
            printf("%s\n", lines[i]);
        }

        exit(EXIT_SUCCESS); 
    }
    else
    {
        wait(NULL);
    }
}

void sort_by_student_name_descending(const char *filename)
{
    // Forks the current process to create a child process.
    pid_t pid = fork();

    // Checks if the fork operation failed.
    if (pid == -1)
    {
        perror("Fork failed.\n"); 
        log_message("Fork failed in sort_by_student_name_descending."); 
        exit(EXIT_FAILURE); 
    }
    else if (pid == 0) // Child process block.
    {
        // Opens the specified file in read-only mode.
        int fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            perror("Error opening file.\n"); 
            log_message("Error opening file in sort_by_student_name_descending."); 
            exit(EXIT_FAILURE); 
        }

        char buffer[MAX_SIZE];
        ssize_t readed_bytes = read(fd, buffer, sizeof(buffer) - 1); // Reads the file into the buffer.
        if (readed_bytes == -1)
        {
            perror("Error reading file.\n"); 
            log_message("Error reading file in sort_by_student_name_descending."); 
            close(fd); 
            exit(EXIT_FAILURE); 
        }
        buffer[readed_bytes] = '\0'; // Ensures null-termination of the string.
        close(fd); 

        char *lines[MAX_SIZE / 20]; // Array to store pointers to the beginning of each line.
        int lines_number = 0; 
        // Splits the buffer into lines.
        char *line = strtok(buffer, "\n");
        while (line != NULL)
        {
            lines[lines_number++] = line; // Stores the pointer to the line.
            line = strtok(NULL, "\n"); // Continues to split the buffer.
        }

        // Sorts the lines using a bubble sort algorithm, modified for descending order.
        for (int i = 0; i < lines_number - 1; i++)
        {
            for (int j = 0; j < lines_number - i - 1; j++)
            {
                if (strcmp(lines[j], lines[j + 1]) < 0)
                {
                    char *temp = lines[j];
                    lines[j] = lines[j + 1];
                    lines[j + 1] = temp;
                }
            }
        }

        log_message("Sorted by student name descending successfully.");

        for (int i = 0; i < lines_number; i++)
        {
            printf("%s\n", lines[i]);
        }

        exit(EXIT_SUCCESS); 
    }
    else
    {
        wait(NULL);
    }
}

int compare_grades_ascending(const void *a, const void *b)
{
    // Predefined order of grades from lowest to highest.
    const char *grade_order[] = {"NA", "VF", "FF", "DD", "DC", "CC", "CB", "BB", "BA", "AA"};
    // Extracts the grade part from each record.
    const char *gradeA = strrchr(*(const char **)a, ' ') + 1;
    const char *gradeB = strrchr(*(const char **)b, ' ') + 1;

    int indexA = 0, indexB = 0;

    // Finds the index of each grade in the predefined order.
    for (int i = 0; i < sizeof(grade_order) / sizeof(grade_order[0]); i++)
    {
        if (strcmp(gradeA, grade_order[i]) == 0)
        {
            indexA = i;
            break;
        }
    }

    for (int i = 0; i < sizeof(grade_order) / sizeof(grade_order[0]); i++)
    {
        if (strcmp(gradeB, grade_order[i]) == 0)
        {
            indexB = i;
            break;
        }
    }

    // Returns the difference in indexes to determine order.
    return indexB - indexA;
}

void sort_by_grade_ascending(const char *filename)
{
    // Opens the specified file in read-only mode.
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file.\n"); 
        log_message("Error opening file in sort_by_grade_ascending.");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_SIZE]; // Buffer to store file contents.
    // Reads the file into the buffer.
    ssize_t readed_bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (readed_bytes == -1)
    {
        perror("Error reading file.\n"); 
        log_message("Error reading file in sort_by_grade_ascending.");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[readed_bytes] = '\0'; // Ensures null-termination.
    close(fd); 

    char *lines[MAX_STUDENTS]; // Array to store pointers to each line.
    int lines_number = 0; // Counter for the number of lines.
    // Splits the buffer into lines (records).
    char *line = strtok(buffer, "\n");
    while (line != NULL)
    {
        lines[lines_number++] = line; // Stores the pointer to each line.
        line = strtok(NULL, "\n"); // Continues to split the buffer.
    }

    // Sorts the lines using the qsort function and compare_grades_ascending for comparison.
    qsort(lines, lines_number, sizeof(char *), compare_grades_ascending);

    log_message("Sorted by grade ascending successfully in sort_by_grade_ascending.");

    for (int i = 0; i < lines_number; ++i)
    {
        printf("%s\n", lines[i]);
    }
}

int compare_grades_descending(const void *a, const void *b)
{
    // Predefined order of grades from highest to lowest for descending sorting.
    const char *grade_order[] = {"NA", "VF", "FF", "DD", "DC", "CC", "CB", "BB", "BA", "AA"};
    // Extracts the grade part from each record.
    const char *gradeA = strrchr(*(const char **)a, ' ') + 1;
    const char *gradeB = strrchr(*(const char **)b, ' ') + 1;

    int indexA = 0, indexB = 0;

    // Finds the index of each grade in the predefined order.
    for (int i = 0; i < sizeof(grade_order) / sizeof(grade_order[0]); i++)
    {
        if (strcmp(gradeA, grade_order[i]) == 0)
        {
            indexA = i;
        }
        if (strcmp(gradeB, grade_order[i]) == 0)
        {
            indexB = i;
        }
    }

    // Returns the difference in indexes for descending order sorting.
    return indexA - indexB;
}

void sort_by_grade_descending(const char *filename)
{
    // Opens the specified file in read-only mode.
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file.\n"); 
        log_message("Error opening file in sort_by_grade_descending.");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_SIZE]; // Buffer to store file contents.
    // Reads the file into the buffer.
    ssize_t readed_bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (readed_bytes == -1)
    {
        perror("Error reading file\n"); 
        log_message("Error reading file in sort_by_grade_descending.");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[readed_bytes] = '\0'; // Ensures null-termination.
    close(fd); // Closes the file descriptor.

    char *lines[MAX_STUDENTS]; // Array to store pointers to each line.
    int lines_number = 0; // Counter for the number of lines.
    // Splits the buffer into lines (records).
    char *line = strtok(buffer, "\n");
    while (line != NULL)
    {
        lines[lines_number++] = line; // Stores the pointer to each line.
        line = strtok(NULL, "\n"); // Continues to split the buffer.
    }

    // Sorts the lines using the qsort function and compare_grades_descending for comparison.
    qsort(lines, lines_number, sizeof(char *), compare_grades_descending);

    log_message("Sorted by grade descending successfully in sort_by_grade_descending.");

    for (int i = 0; i < lines_number; ++i)
    {
        printf("%s\n", lines[i]);
    }
}
