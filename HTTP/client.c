#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_SIZE 10000

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <url>\n", argv[0]);
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    unsigned int port_number = atoi(argv[2]);
    struct sockaddr_in client_address;

    bzero(&client_address, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(port_number);
    if (inet_aton(argv[1], &client_address.sin_addr) == 0) {
        perror(argv[1]);
        exit(-1);
    }

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Socket Creating Error");
        exit(-1);
    }

    if (connect(client_fd, (struct sockaddr *)&client_address, sizeof(struct sockaddr)) < 0) {
        perror("Connection Error");
        exit(-1);
    }

    char *hostname = strtok(argv[3], "/");
    char *file_path = strtok(NULL, "");
    if (file_path == NULL) {
        file_path = "/";
    }
    printf("hostname:%s",hostname);
    printf("path:%s",file_path);
    char request[1024];
    snprintf(request, sizeof(request), "GET %s HTTP/1.0 Host:%s", file_path, hostname);

    if (send(client_fd, request, strlen(request), 0) == -1) {
        perror("Send Error");
        exit(-1);
    }

    FILE *file_ptr;
    char *file_itr = strrchr(file_path, '/');
    if (file_itr == NULL) {
        file_itr = file_path;
    } else {
        file_itr++;
    }

    file_ptr = fopen(file_itr, "w");
    if (file_ptr == NULL) {
        perror("File open error");
        exit(-1);
    }

    int recv_msg_length;
    memset(buffer, 0, BUFFER_SIZE);

    while ((recv_msg_length = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, recv_msg_length, file_ptr);
        memset(buffer, 0, BUFFER_SIZE);
    }

    fclose(file_ptr);
    close(client_fd);
    return 0;
}
