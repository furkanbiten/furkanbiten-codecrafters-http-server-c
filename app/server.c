#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
int endswith(const char* str, const char* suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int main()
{
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int server_fd, connfd, client_addr_len;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(4221),
        .sin_addr = { htonl(INADDR_ANY) },
    };

    if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    int fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    printf("Client connected\n");
    char recv_msg[100];
    recv(fd, &recv_msg, 100, 0);

    // Extract info
    char* req_line = strtok(recv_msg, "\r\n");
    char* header = strtok(NULL, "\r\n");
    char* body = strtok(NULL, "\r\n");

    // Extract http request and url
    char* http = strtok(req_line, " ");
    char* maybe_url = strtok(NULL, " ");

    char error[20];
    if (endswith(maybe_url, (char*)".html") || endswith(maybe_url, (char*)"/")) {
        strcpy(error, "200 OK");
    } else {
        strcpy(error, "404 Not Found");
    }

    char msg[] = "HTTP/1.1 ";
    strcat(msg, error);
    strcat(msg, "\r\n\r\n");

    send(fd, msg, strlen(msg), 0);
    close(server_fd);

    return 0;
}
