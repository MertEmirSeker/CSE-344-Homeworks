#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// Semaphores and shared variables defined globally
sem_t newAutomobile, inChargeforAutomobile;
sem_t newPickup, inChargeforPickup;
int mFree_automobile;     // Number of temporary parking spots for automobiles
int mFree_pickup;         // Number of temporary parking spots for pickups
int permanent_automobile; // Number of permanent parking spots for automobiles
int permanent_pickup;     // Number of permanent parking spots for pickups
int all_spots_full = 0;       // Flag to check if all spots are full

pthread_mutex_t automobile_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for automobile
pthread_mutex_t pickup_mutex = PTHREAD_MUTEX_INITIALIZER;     // Mutex for pickup

void initialize_semaphores()
{
    // Initialize semaphores
    sem_init(&newAutomobile, 0, 1);
    sem_init(&inChargeforAutomobile, 0, 0);
    sem_init(&newPickup, 0, 1);
    sem_init(&inChargeforPickup, 0, 0);

    // Initialize shared variables
    mFree_automobile = 8;     // Initial number of temporary parking spots for automobiles
    mFree_pickup = 4;         // Initial number of temporary parking spots for pickups
    permanent_automobile = 8; // Initial number of permanent parking spots for automobiles
    permanent_pickup = 4;     // Initial number of permanent parking spots for pickups
}

void *carOwner(void *arg)
{
    while (1)
    {
        int vehicleType = rand() % 2; // 0 for pickup, 1 for automobile
        if (vehicleType == 0)
        {                         // Pickup
            sem_wait(&newPickup); // Control access to temporary pickup parking spots
            pthread_mutex_lock(&pickup_mutex);
            if (mFree_pickup > 0)
            {
                mFree_pickup--; // Park a pickup
                printf("A pickup arrives at the parking lot.\n");
                printf("Pickup parked in a temporary spot. Temporary spots left: %d\n", mFree_pickup);
                sem_post(&inChargeforPickup); // Signal valet to move the pickup to a permanent spot
            }
            else
            {
                printf("No temporary spots left for pickups.\n");
            }
            pthread_mutex_unlock(&pickup_mutex);
            sem_post(&newPickup); // Release access
        }
        else
        {                             // Automobile
            sem_wait(&newAutomobile); // Control access to temporary automobile parking spots
            pthread_mutex_lock(&automobile_mutex);
            if (mFree_automobile > 0)
            {
                mFree_automobile--; // Park an automobile
                printf("An automobile arrives at the parking lot.\n");
                printf("Automobile parked in a temporary spot. Temporary spots left: %d\n", mFree_automobile);
                sem_post(&inChargeforAutomobile); // Signal valet to move the automobile to a permanent spot
            }
            else
            {
                printf("No temporary spots left for automobiles.\n");
            }
            pthread_mutex_unlock(&automobile_mutex);
            sem_post(&newAutomobile); // Release access
        }
        sleep(1); // Short delay to avoid immediate consecutive arrivals
    }
    return NULL;
}

void *carAttendant(void *arg)
{
    while (1)
    { // Infinite loop
        // Valet for automobiles
        sem_wait(&inChargeforAutomobile); // Wait for a signal to move an automobile
        sem_wait(&newAutomobile);         // Control access to temporary parking spots
        pthread_mutex_lock(&automobile_mutex);
        if (permanent_automobile > 0)
        {
            permanent_automobile--; // Park the automobile in a permanent spot
            mFree_automobile++;     // Free up the temporary spot
            printf("Automobile moved to a permanent spot by valet. Permanent spots left: %d\n", permanent_automobile);
            printf("Temporary spots for automobiles left after moving: %d\n", mFree_automobile);
        }
        else
        {
            printf("No permanent spots left for automobiles, staying in temporary.\n");
            printf("Temporary spots for automobiles left after moving: %d\n", mFree_automobile);
        }
        pthread_mutex_unlock(&automobile_mutex);
        sem_post(&newAutomobile); // Release access to temporary spots
        usleep(500000);           // 0.5 second delay

        // Valet for pickups
        sem_wait(&inChargeforPickup); // Wait for a signal to move a pickup
        sem_wait(&newPickup);         // Control access to temporary parking spots
        pthread_mutex_lock(&pickup_mutex);
        if (permanent_pickup > 0)
        {
            permanent_pickup--; // Park the pickup in a permanent spot
            mFree_pickup++;     // Free up the temporary spot
            printf("Pickup moved to a permanent spot by valet. Permanent spots left: %d\n", permanent_pickup);
            printf("Temporary spots for pickups left after moving: %d\n", mFree_pickup);
        }
        else
        {
            printf("No permanent spots left for pickups, staying in temporary.\n");
            printf("Temporary spots for pickups left after moving: %d\n", mFree_pickup);
        }
        pthread_mutex_unlock(&pickup_mutex);
        sem_post(&newPickup); // Release access to temporary spots
        usleep(500000);       // 0.5 second delay
    }
    return NULL;
}

void *carExit(void *arg)
{
    while (1)
    {
        sleep(rand() % 5 + 1); // Wait for a random time (between 1 to 5 seconds)

        int vehicleType = rand() % 2; // 0 for pickup, 1 for automobile
        if (vehicleType == 0)
        {                         // Pickup
            sem_wait(&newPickup); // Control access to temporary pickup parking spots
            pthread_mutex_lock(&pickup_mutex);
            if (mFree_pickup < 4)
            {
                mFree_pickup++; // A pickup exits
                printf("Pickup exits from temporary spot. Temporary spots left: %d\n", mFree_pickup);
            }
            pthread_mutex_unlock(&pickup_mutex);
            sem_post(&newPickup); // Release access
        }
        else
        {                             // Automobile
            sem_wait(&newAutomobile); // Control access to temporary automobile parking spots
            pthread_mutex_lock(&automobile_mutex);
            if (mFree_automobile < 8)
            {
                mFree_automobile++; // An automobile exits
                printf("Automobile exits from temporary spot. Temporary spots left: %d\n", mFree_automobile);
            }
            pthread_mutex_unlock(&automobile_mutex);
            sem_post(&newAutomobile); // Release access
        }
    }
    return NULL;
}

void destroy_semaphores()
{
    // Destroy semaphores
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);
}

void signal_handler(int sig)
{
    printf("Interrupt signal received. Exiting...\n");
    destroy_semaphores();
    exit(0);
}

void check_all_spots_full()
{
    pthread_mutex_lock(&automobile_mutex);
    pthread_mutex_lock(&pickup_mutex);
    if (permanent_automobile == 0 && permanent_pickup == 0 &&
        mFree_automobile == 0 && mFree_pickup == 0)
    {
        printf("All parking spots are full. Exiting...\n");
        all_spots_full = 1;
    }
    pthread_mutex_unlock(&automobile_mutex);
    pthread_mutex_unlock(&pickup_mutex);
}

int main()
{
    signal(SIGINT, signal_handler); // Set up handler for SIGINT
    srand(time(NULL));             // Seed the random number generator

    // Initialize semaphores and shared memory
    initialize_semaphores();

    // Define threads
    pthread_t threads[6]; // Total of 6 threads: 2 carOwner, 2 carAttendant, 2 carExit
    int i;

    // Create carOwner threads
    for (i = 0; i < 2; i++)
    {
        if (pthread_create(&threads[i], NULL, carOwner, NULL) != 0)
        {
            perror("Failed to create carOwner thread");
            return 1;
        }
    }

    // Create carAttendant threads
    for (i = 2; i < 4; i++)
    {
        if (pthread_create(&threads[i], NULL, carAttendant, NULL) != 0)
        {
            perror("Failed to create carAttendant thread");
            return 1;
        }
    }

    // Create carExit threads
    for (i = 4; i < 6; i++)
    {
        if (pthread_create(&threads[i], NULL, carExit, NULL) != 0)
        {
            perror("Failed to create carExit thread");
            return 1;
        }
    }

    // Check if all parking spots are full
    while (!all_spots_full)
    {
        sleep(1);              // Check every second
        check_all_spots_full(); // Check the occupancy of parking spots
        if (all_spots_full)
        {
            for (int j = 0; j < 6; j++)
            {
                pthread_cancel(threads[j]); // Cancel all threads
            }
            break;
        }
    }

    // Wait for all threads to finish
    for (i = 0; i < 6; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Destroy semaphores and shared memory
    destroy_semaphores();

    printf("All threads have finished. Program terminating.\n");
    return 0;
}
