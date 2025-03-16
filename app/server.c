#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static char* tmp_path;

void handle_client(int client_fd)
{
    // Read HTTP request
    char buffer[1024];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        printf("Read failed: %s \n", strerror(errno));
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';
    // Extract URL path from request line
    char method[16], path[256], version[16];
    sscanf(buffer, "%s %s %s", method, path, version);

    // Check if the path is /user-agent
    if (strcmp(path, "/user-agent") == 0) {
        // Find the User-Agent header
        char* user_agent = strstr(buffer, "User-Agent:");
        if (user_agent) {
            user_agent += strlen("User-Agent: ");
            char* end_of_line = strstr(user_agent, "\r\n");

            if (end_of_line) {
                *end_of_line = '\0';
                char response[1024];
                int response_length = snprintf(response, sizeof(response),
                    "HTTP/1.1 200 OK\r\nContent-Type: "
                    "text/plain\r\nContent-Length: %zu\r\n\r\n%s",
                    strlen(user_agent), user_agent);
                write(client_fd, response, response_length);
            }
        } else {
            const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            write(client_fd, response, strlen(response));
        }
    } else if (strncmp(path, "/files/", 7) == 0) {
        char file_path[100] = "";
        strcat(file_path, tmp_path);
        strcat(file_path, path + 7);
        printf("file_path: %s", file_path);
        FILE* fp = fopen(file_path, "r");

        if (fp == NULL) {
            write(client_fd, "HTTP/1.1 404 Not Found\r\n\r\n", 100);
        } else {
            char response[1024];
            char buffer[256];
            while (fgets(buffer, 1024, fp))
                ;

            int response_length = snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\nContent-Type: "
                "application/octet-stream\r\nContent-Length: %zu\r\n\r\n%s",
                strlen(buffer), buffer);
            write(client_fd, response, response_length);
        }

    } else if (strncmp(path, "/echo/", 6) == 0) {
        const char* echo_str = path + 6;
        char response[1024];
        int response_length = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\nContent-Type: "
            "text/plain\r\nContent-Length: %zu\r\n\r\n%s",
            strlen(echo_str), echo_str);
        write(client_fd, response, response_length);
    } else if (strcmp(path, "/") == 0) {
        const char* response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
                               "13\r\n\r\nHello, world!";
        write(client_fd, response, strlen(response));
    } else {
        const char* response = "HTTP/1.1 404 Not Found\r\n\r\n";
        write(client_fd, response, strlen(response));
    }
    close(client_fd);
}

int main(int argc, char** argv)
{
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");
    int server_fd, client_fd, client_addr_len;

    printf("argc: %d, argv: %s\n", argc, tmp_path);
    if (argc > 1 && strcmp(argv[1], "--directory") == 0) {
        tmp_path = argv[2];
    }

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
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_fd < 0) {
            printf("Accept failed: %s \n", strerror(errno));
            continue;
        }
        printf("Client connected\n");

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd);
            handle_client(client_fd);
            exit(0);
        } else if (pid > 0) {
            // Parent process
            close(client_fd);
            waitpid(-1, NULL, WNOHANG); // Clean up zombie processes
        } else {
            printf("Fork failed: %s \n", strerror(errno));
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
