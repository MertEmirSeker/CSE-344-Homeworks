#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 2048

int client_socket;
int *client_sockets = NULL; // All client sockets
int number_of_clients = 0; // Number of clients
void handle_signal(int signal);

void receive_completion_status(const char* ipaddress, int port);

int main(int argc, char* argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Kullanım: %s [ipaddress] [port] [numberOfClients] [p] [q]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* ipaddress = argv[1];
    int port = atoi(argv[2]);
    number_of_clients = atoi(argv[3]);
    int p = atoi(argv[4]);
    int q = atoi(argv[5]);

    srand(time(NULL));

    printf("> PID %d.. \n", getpid());

    // Sinyal işleme ayarları
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    client_sockets = (int*)malloc(number_of_clients * sizeof(int));

    // First, send the number of clients and p, q values to the server
    int init_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (init_socket < 0) {
        perror("Soket oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port); // Kullanıcıdan alınan port numarası
    server_address.sin_addr.s_addr = inet_addr(ipaddress); // Kullanıcıdan alınan IP adresi

    if (connect(init_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Sunucuya bağlanılamadı");
        close(init_socket);
        exit(EXIT_FAILURE);
    }

    send(init_socket, &number_of_clients, sizeof(int), 0); // Send number of clients to server
    send(init_socket, &p, sizeof(int), 0); // Send p value
    send(init_socket, &q, sizeof(int), 0); // Send q value
    close(init_socket);

    for (int i = 0; i < number_of_clients; ++i) {
        client_sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sockets[i] < 0) {
            perror("Soket oluşturulamadı");
            exit(EXIT_FAILURE);
        }

        if (connect(client_sockets[i], (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
            perror("Sunucuya bağlanılamadı");
            close(client_sockets[i]);
            exit(EXIT_FAILURE);
        }

        pid_t client_pid = getpid(); // Get current process ID (PID)
        send(client_sockets[i], &client_pid, sizeof(pid_t), 0); // Send PID to server

        int order_id = i + 1;
        int customer_x = rand() % p;
        int customer_y = rand() % q;
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "%d %d %d %d", order_id, customer_x, customer_y, client_pid);

        send(client_sockets[i], buffer, strlen(buffer), 0);
        printf("> Sipariş verildi: ID %d, Konum (%d, %d)\n", order_id, customer_x, customer_y);

  

        close(client_sockets[i]);
    }

    // Siparişlerin tamamlandığını bekle
    receive_completion_status(ipaddress, port + 2);

    printf("> Tüm siparişler tamamlandı\n");
    free(client_sockets);
    return 0;
}

void receive_completion_status(const char* ipaddress, int port) {
    int completion_socket;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];

    completion_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (completion_socket < 0) {
        perror("Soket oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port); // Sipariş tamamlanma durumlarını dinleyecek port
    server_address.sin_addr.s_addr = inet_addr(ipaddress); // Kullanıcıdan alınan IP adresi

    if (connect(completion_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Sunucuya bağlanılamadı");
        close(completion_socket);
        exit(EXIT_FAILURE);
    }

    while (1) {
        int valread = read(completion_socket, buffer, BUFFER_SIZE);
        if (valread > 0) {
            buffer[valread] = '\0';
            printf("> %s\n", buffer);
            if (strcmp(buffer, "All orders completed") == 0) {
                // Mesajı aldıktan sonra istemciyi kapat
                close(completion_socket);
                break;
            }
        }
    }
}

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        printf("\n> ^C sinyali alındı.. siparişler iptal ediliyor..\n");
        // Sipariş iptali işlemleri
        for (int i = 0; i < number_of_clients; ++i) {
            if (client_sockets[i] != -1) {
                close(client_sockets[i]);
            }
        }
        free(client_sockets);
        exit(EXIT_SUCCESS);
    }
}
