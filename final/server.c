#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <time.h>
#include <sys/time.h>

#define MAX_COOKS 10
#define MAX_DELIVERIES 10
#define MAX_CLIENTS 100
#define OVEN_CAPACITY 6
#define APPARATUS 3
#define BAG_CAPACITY 3
#define BUFFER_SIZE 1024
#define ROWS 30
#define COLS 40

typedef struct {
    int order_id;
    int customer_x;
    int customer_y;
    int state; // 0: placed, 1: prepared, 2: cooked, 3: delivering, 4: completed, 5: cancelled
    int cook_id; // Pişiren aşçı kimliği
    pid_t pid; // Client PID
} Order;

typedef struct {
    pthread_t thread_id;
    int id;
    int work_count; // Aşçının kaç kez çalıştığını izlemek için sayaç
} Cook;

typedef struct {
    pthread_t thread_id;
    int id;
    int delivery_count;
    int current_orders; // Number of current orders the delivery person is carrying
    int work_count; // Teslimatçının kaç kez çalıştığını izlemek için sayaç
    Order orders[BAG_CAPACITY]; // Array to store the orders
} DeliveryPerson;

typedef struct QueueNode {
    Order order;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count; // Queue'daki öğe sayısını izlemek için sayaç
} Queue;

void init_queue(Queue* q) {
    q->front = q->rear = NULL;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void enqueue(Queue* q, Order order) {
    QueueNode* temp = (QueueNode*)malloc(sizeof(QueueNode));
    temp->order = order;
    temp->next = NULL;

    pthread_mutex_lock(&q->mutex);
    if (q->rear == NULL) {
        q->front = q->rear = temp;
    } else {
        q->rear->next = temp;
        q->rear = temp;
    }
    q->count++;
    pthread_cond_signal(&q->cond); // Bekleyen bir thread'i uyandır
    pthread_mutex_unlock(&q->mutex);
}

Order dequeue(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->front == NULL) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    QueueNode* temp = q->front;
    Order order = temp->order;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    free(temp);
    return order;
}

void free_queue(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    QueueNode* current = q->front;
    while (current != NULL) {
        QueueNode* temp = current;
        current = current->next;
        free(temp);
    }
    q->front = q->rear = NULL;
    q->count = 0;
    pthread_mutex_unlock(&q->mutex);
}

int get_queue_size(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    int size = q->count;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

Order* orders = NULL;
size_t order_capacity = 0;
Cook* cooks;
DeliveryPerson* delivery_personnel;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t completion_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t completion_cond = PTHREAD_COND_INITIALIZER;
int active_orders = 0;
int total_orders = 0;
int completed_orders = 0;
int oven_occupancy = 0;
int apparatus_available = APPARATUS;
int *delivery_times;
int delivery_speed;
int status_socket;
int completion_socket = -1;
struct sockaddr_in status_address;
struct sockaddr_in completion_address;
int p, q; // Haritanın boyutları

int pending_deliveries = 0; // Aktif teslimat sayısı
pthread_mutex_t pending_deliveries_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for pending deliveries

Queue client_queues[MAX_CLIENTS]; // Maksimum istemci sayısına göre ayarlanabilir
int client_count = 0;

pthread_mutex_t expand_mutex = PTHREAD_MUTEX_INITIALIZER; // Yeni mutex kilidi

void expand_order_array() {
    pthread_mutex_lock(&expand_mutex); // Kilidi al
    size_t new_capacity = (order_capacity == 0) ? 10 : order_capacity * 2;
    orders = realloc(orders, new_capacity * sizeof(Order));
    if (orders == NULL) {
        perror("Failed to expand order array");
        exit(EXIT_FAILURE);
    }
    order_capacity = new_capacity;
    pthread_mutex_unlock(&expand_mutex); // Kilidi bırak
}

void increment_pending_deliveries() {
    pthread_mutex_lock(&pending_deliveries_mutex);
    pending_deliveries++;
    pthread_mutex_unlock(&pending_deliveries_mutex);
}

void decrement_pending_deliveries() {
    pthread_mutex_lock(&pending_deliveries_mutex);
    pending_deliveries--;
    pthread_cond_signal(&delivery_cond);
    pthread_mutex_unlock(&pending_deliveries_mutex);
}

void wait_for_all_deliveries() {
    pthread_mutex_lock(&pending_deliveries_mutex);
    while (pending_deliveries > 0 || completed_orders < total_orders) {
        pthread_cond_wait(&delivery_cond, &pending_deliveries_mutex);
    }
    pthread_mutex_unlock(&pending_deliveries_mutex);
}

void check_and_expand_order_array() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        int queue_size = get_queue_size(&client_queues[i]);
        if (queue_size > order_capacity) {
            expand_order_array(); // Orders dizisini genişletme işlemi
        }
    }
}

// Rastgele karmaşık sayı matris oluşturma
void create_matrix(complex double matrix[ROWS][COLS]) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            matrix[i][j] = (complex double)rand() / RAND_MAX + (complex double)rand() / RAND_MAX * I;
        }
    }
}

// Matris çarpma (C = A * B)
void matrix_multiply(complex double A[ROWS][COLS], complex double B[COLS][ROWS], complex double C[ROWS][ROWS]) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < ROWS; j++) {
            C[i][j] = 0.0 + 0.0 * I;
            for (int k = 0; k < COLS; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

void matrix_multiply_alt(complex double A[COLS][ROWS], complex double B[ROWS][COLS], complex double C[COLS][COLS]) {
    for (int i = 0; i < COLS; i++) {
        for (int j = 0; j < COLS; j++) {
            C[i][j] = 0.0 + 0.0 * I;
            for (int k = 0; k < ROWS; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Matris transpoz (B = A^T)
void matrix_transpose(complex double A[ROWS][COLS], complex double B[COLS][ROWS]) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            B[j][i] = A[i][j];
        }
    }
}

// Matris tersini hesaplama (gaussian elimination)
int matrix_inverse(complex double A[COLS][COLS], complex double B[COLS][COLS]) {
    int i, j, k;
    complex double ratio;
    complex double temp;
    complex double augmented[COLS][2 * COLS];

    // Augmenting Identity Matrix of Order n
    for (i = 0; i < COLS; i++) {
        for (int j = 0; j < COLS; j++) {
            augmented[i][j] = A[i][j];
            augmented[i][j + COLS] = (i == j) ? 1.0 + 0.0 * I : 0.0 + 0.0 * I;
        }
    }

    // Applying Gauss Jordan Elimination
    for (i = 0; i < COLS; i++) {
        if (cabs(augmented[i][i]) == 0.0) {
            printf("Mathematical Error!");
            return 0;
        }
        for (j = 0; j < COLS; j++) {
            if (i != j) {
                ratio = augmented[j][i] / augmented[i][i];
                for (k = 0; k < 2 * COLS; k++) {
                    augmented[j][k] -= ratio * augmented[i][k];
                }
            }
        }
    }

    // Row Operation to Make Principal Diagonal to 1
    for (i = 0; i < COLS; i++) {
        temp = augmented[i][i];
        for (int j = 0; j < 2 * COLS; j++) {
            augmented[i][j] /= temp;
        }
    }

    // Extracting inverse matrix
    for (i = 0; i < COLS; i++) {
        for (int j = 0; j < COLS; j++) {
            B[i][j] = augmented[i][j + COLS];
        }
    }

    return 1;
}

// Pseudo-ters hesaplama (Moore-Penrose yöntemi ile)
void calculate_pseudo_inverse(complex double matrix[ROWS][COLS], complex double inverse[COLS][ROWS]) {
    complex double matrix_T[COLS][ROWS];
    complex double matrix_T_mul_matrix[COLS][COLS];
    complex double inverse_matrix_T_mul_matrix[COLS][COLS];

    // A^T
    matrix_transpose(matrix, matrix_T);

    // A^T * A
    matrix_multiply_alt(matrix_T, matrix, matrix_T_mul_matrix);

    // (A^T * A)^-1
    if (!matrix_inverse(matrix_T_mul_matrix, inverse_matrix_T_mul_matrix)) {
        printf("Matrix inversion failed!\n");
        return;
    }

    // A^+ = (A^T * A)^-1 * A^T
    for (int i = 0; i < COLS; i++) {
        for (int j = 0; j < ROWS; j++) {
            inverse[i][j] = 0.0 + 0.0 * I;
            for (int k = 0; k < COLS; k++) {
                inverse[i][j] += inverse_matrix_T_mul_matrix[i][k] * matrix_T[k][j];
            }
        }
    }
}

// Aşçı çalışma süresi hesaplama
double calculate_cook_time() {
    complex double matrix[ROWS][COLS];
    complex double inverse[COLS][ROWS];
    create_matrix(matrix);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    // Pseudo-ters hesaplama
    for (int k = 0; k < 10; k++) {  // Daha uzun süreli bir hesaplama simülasyonu için döngü
        calculate_pseudo_inverse(matrix, inverse);
    }

    gettimeofday(&end_time, NULL);

    double cook_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
    return cook_time;
}

// Pişirme süresi hesaplama
double calculate_bake_time(double cook_time) {
    return cook_time / 2; // Pişirme süresi, hazırlık süresinin yarısı
}

void* handle_client(void* arg);
void* cook_function(void* arg);
void* delivery_function(void* arg);
void handle_signal(int signal);
void log_activity(const char* message, const char* mode);
void* handle_status_updates(void* arg);
void* handle_completion_updates(void* arg);

void* handle_client_queue(void* arg) {
    int client_index = *(int*)arg;
    Queue* q = &client_queues[client_index];
    free(arg);
    char log_msg[256];

    while (1) {
        Order order = dequeue(q);

        // Siparişi hazırlama ve pişirme
        double prepare_time = calculate_cook_time();
        usleep((int)(prepare_time * 1000000));  // microseconds sleep

        pthread_mutex_lock(&order_mutex);
        while (apparatus_available == 0 || oven_occupancy == OVEN_CAPACITY) {
            pthread_cond_wait(&order_cond, &order_mutex);
        }
        apparatus_available--;
        oven_occupancy++;
        order.state = 2;
        pthread_mutex_unlock(&order_mutex);

        double bake_time = calculate_bake_time(prepare_time);
        usleep((int)(bake_time * 1000000));  // microseconds sleep

        pthread_mutex_lock(&order_mutex);
        oven_occupancy--;
        apparatus_available++;
        order.state = 3;
        pthread_cond_signal(&order_cond);
        pthread_mutex_unlock(&order_mutex);

        // Pişirme süresini log'a yaz
        snprintf(log_msg, sizeof(log_msg), "> Cook %d cooked order %d in %.7f seconds", order.cook_id, order.order_id, bake_time);
        printf("%s\n", log_msg);
        log_activity(log_msg, "a");

        // Teslimat kuyruğuna ekle
        pthread_mutex_lock(&delivery_mutex);
        enqueue(&client_queues[client_index], order);
        pthread_cond_signal(&delivery_cond);
        pthread_mutex_unlock(&delivery_mutex);
    }

    return NULL;
}

void cleanup() {
    // Free allocated memory for orders, cooks, delivery personnel, and delivery times
    free(orders);
    free(cooks);
    free(delivery_personnel);
    free(delivery_times);

    // Free all client queues
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        free_queue(&client_queues[i]);
    }

    // Close sockets
    if (status_socket != -1) {
        close(status_socket);
    }
    if (completion_socket != -1) {
        close(completion_socket);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [ipaddress] [port] [CookthreadPoolSize] [DeliveryPoolSize] [k]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* ipaddress = argv[1];
    int port = atoi(argv[2]);
    int cook_pool_size = atoi(argv[3]);
    int delivery_pool_size = atoi(argv[4]);
    delivery_speed = atoi(argv[5]);

    orders = NULL;
    order_capacity = 0;
    cooks = (Cook*) malloc(cook_pool_size * sizeof(Cook));
    delivery_personnel = (DeliveryPerson*) malloc(delivery_pool_size * sizeof(DeliveryPerson));
    delivery_times = (int*) malloc(delivery_pool_size * sizeof(int));

    for (int i = 0; i < cook_pool_size; ++i) {
        cooks[i].id = i;
        cooks[i].work_count = 0; // Aşçı iş sayacını başlat
    }

    for (int i = 0; i < delivery_pool_size; ++i) {
        delivery_personnel[i].id = i;
        delivery_personnel[i].delivery_count = 0;
        delivery_personnel[i].current_orders = 0;
        delivery_personnel[i].work_count = 0; // Teslimatçı iş sayacını başlat
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        init_queue(&client_queues[i]);
    }

    // Initialize server socket
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ipaddress); // Kullanıcıdan alınan IP adresi
    address.sin_port = htons(port); // Kullanıcıdan alınan port numarası

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Initialize status update socket
    if ((status_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("status socket failed");
        exit(EXIT_FAILURE);
    }

    status_address.sin_family = AF_INET;
    status_address.sin_addr.s_addr = inet_addr(ipaddress);
    status_address.sin_port = htons(port + 1); // Statü portu için bir sonraki port

    if (bind(status_socket, (struct sockaddr *)&status_address, sizeof(status_address)) < 0) {
        perror("bind failed for status socket");
        exit(EXIT_FAILURE);
    }
    if (listen(status_socket, 3) < 0) {
        perror("listen failed for status socket");
        exit(EXIT_FAILURE);
    }

    // Initialize completion update socket
    if ((completion_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("completion socket failed");
        exit(EXIT_FAILURE);
    }

    completion_address.sin_family = AF_INET;
    completion_address.sin_addr.s_addr = inet_addr(ipaddress);
    completion_address.sin_port = htons(port + 2); // Tamamlanma portu için iki sonraki port

    if (bind(completion_socket, (struct sockaddr *)&completion_address, sizeof(completion_address)) < 0) {
        perror("bind failed for completion socket");
        exit(EXIT_FAILURE);
    }
    if (listen(completion_socket, 3) < 0) {
        perror("listen failed for completion socket");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    pthread_t* cook_threads = malloc(cook_pool_size * sizeof(pthread_t));
    pthread_t* delivery_threads = malloc(delivery_pool_size * sizeof(pthread_t));

    for (int i = 0; i < cook_pool_size; ++i) {
        pthread_create(&cook_threads[i], NULL, cook_function, (void*)&cooks[i]);
    }

    for (int i = 0; i < delivery_pool_size; ++i) {
        pthread_create(&delivery_threads[i], NULL, delivery_function, (void*)&delivery_personnel[i]);
    }

    pthread_t status_thread, completion_thread;
    pthread_create(&status_thread, NULL, handle_status_updates, NULL);
    pthread_create(&completion_thread, NULL, handle_completion_updates, NULL);

    printf("> PideShop active waiting for connection ...\n");

    // Log dosyasını başlangıçta temizle
    log_activity("", "w");

    while (1) {
        check_and_expand_order_array(); // Orders dizisini kontrol et ve gerekirse genişlet

        int customer_count = 0;
        int *client_sockets = NULL;
        pid_t *client_pids = NULL; // Store PIDs of clients
        pid_t last_client_pid; // Store the PID of the last client

        // Accept an initial connection to get the number of clients
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Read the number of clients
        int number_of_clients;
        read(new_socket, &number_of_clients, sizeof(int));
        // Read p and q values
        read(new_socket, &p, sizeof(int));
        read(new_socket, &q, sizeof(int));
        close(new_socket);

        // Allocate memory for client sockets and PIDs based on the number of clients
        client_sockets = (int *)malloc(number_of_clients * sizeof(int));
        client_pids = (pid_t *)malloc(number_of_clients * sizeof(pid_t));

        for (int i = 0; i < number_of_clients; ++i) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            client_sockets[i] = new_socket;
            read(new_socket, &client_pids[i], sizeof(pid_t)); // Read client PID from socket
            last_client_pid = client_pids[i]; // Update the last client PID
            customer_count++;
        }

        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "\n------ Client PID: %d connected. ------", client_pids[0]);
        printf("%s\n", log_msg);
        log_activity(log_msg, "a");

        char customer_log[256];
        snprintf(customer_log, sizeof(customer_log), "> %d new customers.. Serving", customer_count);
        printf("%s\n", customer_log);
        log_activity(customer_log, "a");

        int* client_index = malloc(sizeof(int));
        *client_index = client_count++;
        for (int i = 0; i < number_of_clients; ++i) {
            int* client_socket = malloc(sizeof(int));
            *client_socket = client_sockets[i];
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, handle_client, client_socket);
            pthread_detach(client_thread);
        }

        pthread_t client_queue_thread;
        pthread_create(&client_queue_thread, NULL, handle_client_queue, client_index);
        pthread_detach(client_queue_thread);

        // Wait until all orders are completed
        pthread_mutex_lock(&completion_mutex);
        while (completed_orders < total_orders) {
            pthread_cond_wait(&completion_cond, &completion_mutex);
        }
        pthread_mutex_unlock(&completion_mutex);

        // Wait for all deliveries to be completed
        wait_for_all_deliveries();

        // En fazla çalışan cook ve delivery person'u bul
        int max_cook_work = 0;
        int max_cook_id = -1;
        for (int i = 0; i < cook_pool_size; ++i) {
            if (cooks[i].work_count > max_cook_work) {
                max_cook_work = cooks[i].work_count;
                max_cook_id = cooks[i].id;
            }
        }

        int max_delivery_work = 0;
        int max_delivery_id = -1;
        for (int i = 0; i < delivery_pool_size; ++i) {
            if (delivery_personnel[i].delivery_count > max_delivery_work) {
                max_delivery_work = delivery_personnel[i].work_count;
                max_delivery_id = delivery_personnel[i].id;
            }
        }

        // En fazla çalışan cook ve delivery person'u yazdır
        printf("> Most hardworking cook: Cook %d with %d orders prepared and cooked\n", max_cook_id, max_cook_work);
        printf("> Most hardworking delivery person: Delivery Person %d with %d deliveries\n", max_delivery_id, max_delivery_work);

        printf("> done serving client @ XXX PID %d\n", last_client_pid);
        printf("> active waiting for connections\n");


        // Reset counters for next batch of customers
        total_orders = 0;
        completed_orders = 0;
        active_orders = 0;

        // Free allocated memory for client sockets and PIDs
        free(client_sockets);
        free(client_pids);
    }

    // Cleanup threads
    for (int i = 0; i < cook_pool_size; ++i) {
        pthread_join(cook_threads[i], NULL);
    }

    for (int i = 0; i < delivery_pool_size; ++i) {
        pthread_join(delivery_threads[i], NULL);
    }

    pthread_join(status_thread, NULL);
    pthread_join(completion_thread, NULL);

    // Cleanup
    cleanup();

    return 0;
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE] = {0};
    int valread = read(client_socket, buffer, BUFFER_SIZE);
    if (valread < 0) {
        perror("read");
        close(client_socket);
        return NULL;
    }

    // Process client request and manage orders
    int order_id, customer_x, customer_y;
    pid_t client_pid;
    sscanf(buffer, "%d %d %d %d", &order_id, &customer_x, &customer_y, &client_pid);

    pthread_mutex_lock(&order_mutex);
    if (total_orders >= order_capacity) {
        expand_order_array(); // Orders dizisini genişletme işlemi
    }
    orders[total_orders].order_id = order_id;
    orders[total_orders].customer_x = customer_x;
    orders[total_orders].customer_y = customer_y;
    orders[total_orders].state = 0;
    orders[total_orders].pid = client_pid;
    total_orders++;
    active_orders++;
    pthread_cond_signal(&order_cond);
    pthread_mutex_unlock(&order_mutex);

    close(client_socket);
    return NULL;
}

void* cook_function(void* arg) {
    Cook* cook = (Cook*)arg;
    char log_msg[256];

    while (1) {
        pthread_mutex_lock(&order_mutex);
        while (active_orders == 0) {
            pthread_cond_wait(&order_cond, &order_mutex);
        }

        int order_index = -1;
        for (int i = 0; i < total_orders; ++i) {
            if (orders[i].state == 0) {
                order_index = i;
                break;
            }
        }

        if (order_index != -1) {
            orders[order_index].state = 1;
            orders[order_index].cook_id = cook->id; // Aşçının kimliğini sakla
            cook->work_count++; // Aşçı iş sayacını artır
            pthread_mutex_unlock(&order_mutex);

            // Hazırlama süresi hesaplama ve simülasyon
            double prepare_time = calculate_cook_time();
            usleep((int)(prepare_time * 1000000));  // microseconds sleep

            // Hazırlama süresini log'a yaz
            snprintf(log_msg, sizeof(log_msg), "> Cook %d prepared order %d in %.7f seconds", cook->id, orders[order_index].order_id, prepare_time);
            printf("%s\n", log_msg);
            log_activity(log_msg, "a");

            pthread_mutex_lock(&order_mutex);
            while (apparatus_available == 0 || oven_occupancy == OVEN_CAPACITY) {
                pthread_cond_wait(&order_cond, &order_mutex);
            }
            apparatus_available--;
            oven_occupancy++;
            orders[order_index].state = 2;
            pthread_mutex_unlock(&order_mutex);

            // Pişirme süresi hesaplama ve simülasyon
            double bake_time = calculate_bake_time(prepare_time);
            usleep((int)(bake_time * 1000000));  // microseconds sleep

            pthread_mutex_lock(&order_mutex);
            oven_occupancy--;
            apparatus_available++;
            orders[order_index].state = 3;
            pthread_cond_signal(&order_cond);
            pthread_mutex_unlock(&order_mutex);

            // Pişirme süresini log'a yaz
            snprintf(log_msg, sizeof(log_msg), "> Cook %d cooked order %d in %.7f seconds", cook->id, orders[order_index].order_id, bake_time);
            printf("%s\n", log_msg);
            log_activity(log_msg, "a");

            pthread_mutex_lock(&delivery_mutex);
            pthread_cond_signal(&delivery_cond);
            pthread_mutex_unlock(&delivery_mutex);
        } else {
            pthread_mutex_unlock(&order_mutex);
        }
    }

    return NULL;
}

void* delivery_function(void* arg) {
    DeliveryPerson* delivery_person = (DeliveryPerson*)arg;
    char log_msg[256];

    while (1) {
        pthread_mutex_lock(&delivery_mutex);

        // Kuryenin çantası dolana kadar bekle
        while (delivery_person->current_orders < BAG_CAPACITY && active_orders > 0) {
            // Teslim edilmek üzere olan bir sipariş bulun
            int order_found = 0;
            for (int i = 0; i < total_orders; ++i) {
                if (orders[i].state == 3) {
                    orders[i].state = 4; // Siparişin durumunu güncelle
                    delivery_person->orders[delivery_person->current_orders++] = orders[i];
                    delivery_person->work_count++; // Teslimatçı iş sayacını artır
                    active_orders--;
                    order_found = 1;
                    break;
                }
            }

            // Eğer hazır sipariş yoksa bekle
            if (!order_found) {
                pthread_cond_wait(&delivery_cond, &delivery_mutex);
            }
        }

        pthread_mutex_unlock(&delivery_mutex);

        // Eğer en az bir sipariş varsa çık ve teslim et
        if (delivery_person->current_orders > 0) {
            increment_pending_deliveries(); // Aktif teslimat sayısını artır

            // Teslim alma işlemini logla
            snprintf(log_msg, sizeof(log_msg), "> Delivery Person %d took orders:", delivery_person->id);
            for (int i = 0; i < delivery_person->current_orders; ++i) {
                snprintf(log_msg + strlen(log_msg), sizeof(log_msg) - strlen(log_msg), " %d,", delivery_person->orders[i].order_id);
            }
            printf("%s\n", log_msg);
            log_activity(log_msg, "a");

            for (int i = 0; i < delivery_person->current_orders; ++i) {
                // Teslimat süresini simüle et
                int distance = sqrt(pow(delivery_person->orders[i].customer_x - p / 2, 2) + pow(delivery_person->orders[i].customer_y - q / 2, 2)); // Haritanın ortasındaki dükkan
                int delivery_time = distance / delivery_speed; // Basitleştirilmiş teslimat süresi hesaplaması
                printf("Delivery time: %d seconds\n", delivery_time);
                sleep(delivery_time);

                delivery_person->delivery_count++;
                snprintf(log_msg, sizeof(log_msg), "> Delivery Person %d delivered order %d to location (%d, %d) and Thanks Cook %d and Moto %d", delivery_person->id, delivery_person->orders[i].order_id, delivery_person->orders[i].customer_x, delivery_person->orders[i].customer_y, delivery_person->orders[i].cook_id, delivery_person->id);
                printf("%s\n", log_msg);
                log_activity(log_msg, "a");

                // Durum güncellemesini status soket'e gönder
                int status_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (status_fd < 0) {
                    perror("status socket creation failed");
                } else {
                    if (connect(status_fd, (struct sockaddr*)&status_address, sizeof(status_address)) == 0) {
                        send(status_fd, log_msg, strlen(log_msg), 0);
                    } else {
                        perror("status socket connection failed");
                    }
                    close(status_fd);
                }

                pthread_mutex_lock(&order_mutex);
                delivery_person->orders[i].state = 4;
                completed_orders++;
                pthread_mutex_unlock(&order_mutex);
            }



            // Teslimatçı siparişlerini sıfırla
            delivery_person->current_orders = 0;

            // Eğer tüm siparişler tamamlandıysa tamamlanma sinyali gönder
            pthread_mutex_lock(&completion_mutex);
            pthread_cond_signal(&completion_cond);
            pthread_mutex_unlock(&completion_mutex);

            decrement_pending_deliveries(); // Aktif teslimat sayısını azalt
        }
    }

    return NULL;
}

void* handle_status_updates(void* arg) {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        if ((new_socket = accept(status_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("status socket accept failed");
            continue;
        }

        char buffer[BUFFER_SIZE];
        int valread;
        while ((valread = read(new_socket, buffer, BUFFER_SIZE)) > 0) {
            buffer[valread] = '\0';
         //   printf("Status Update: %s\n", buffer);
        }

        close(new_socket);
    }

    return NULL;
}

void* handle_completion_updates(void* arg) {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        if ((new_socket = accept(completion_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("completion socket accept failed");
            continue;
        }

        pthread_mutex_lock(&completion_mutex);
        while (completed_orders < total_orders) {
            pthread_cond_wait(&completion_cond, &completion_mutex);
        }
        pthread_mutex_unlock(&completion_mutex);

        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE, "All orders completed");

        send(new_socket, buffer, strlen(buffer), 0);
        close(new_socket);
    }

    return NULL;
}

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        printf("\n> ^C.. Upps quitting.. writing log file\n");
        log_activity("> Server shut down", "a");

        // Free all allocated memory
        cleanup();

        exit(EXIT_SUCCESS);
    }
}

void log_activity(const char* message, const char* mode) {
    FILE* log_file = fopen("pide_shop.log", mode);
    if (log_file == NULL) {
        perror("fopen");
        return;
    }
    fprintf(log_file, "%s\n", message);
    fclose(log_file);
}
