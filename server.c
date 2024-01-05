#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define BUFFER_SIZE 4096

void handle_client_request(int client_fd);

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);

    // Create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Define the server address
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);  // Using port 8080
    addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port 8080...\n");

    // Accept client connections and handle them
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, (socklen_t *)&addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        handle_client_request(client_fd);
        close(client_fd);
    }

    return 0;
}

void handle_client_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_read < 0) {
        perror("Error reading request");
        return;
    }

    // Parse HTTP method and path
    char method[8], path[256];
    sscanf(buffer, "%s %s", method, path);  

    // Only handling GET requests for simplicity
    if (strcmp(method, "GET") == 0) {
        // Open and send the requested file
        int file_fd = open(path + 1, O_RDONLY); // +1 to skip the leading '/'
        if (file_fd < 0) {
            // File not found, send 404 Not Found response
            char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client_fd, response, strlen(response), 0);
        } else {
            // File found, send 200 OK response followed by file content
            char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
            send(client_fd, header, strlen(header), 0);
            sendfile(client_fd, file_fd, NULL, BUFFER_SIZE);
            close(file_fd);
        }
    } else {
        // Method not implemented, send 501 Not Implemented response
        char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
    }
}
