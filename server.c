#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <pthread.h>

#define BUFFER_SIZE 4096

void *handle_client_request(void *client_fd_ptr);

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);

    // Create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Define the server address
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080); // Using port 8080
    addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port 8080...\n");

    // Accept client connections and handle them
    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, (socklen_t *)&addr_len);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        // Spawn a new thread for each client connection
        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        if (pthread_create(&thread_id, NULL, handle_client_request, client_fd_ptr) != 0)
        {
            perror("Failed to create thread");
        }
        pthread_detach(thread_id);
    }

    return 0;
}

void *handle_client_request(void *client_fd_ptr)
{
    int client_fd = *((int *)client_fd_ptr);
    free(client_fd_ptr);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read < 0)
    {
        perror("Error reading request");
        return;
    }

    printf("Received request: %s\n", buffer); // Debug print

    char method[8], path[256], protocol[16];
    sscanf(buffer, "%s %s %s", method, path, protocol);

    if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0)
    {
        // Handle GET and HEAD (HEAD is identical to GET but without response body)
        int file_fd = open(path + 1, O_RDONLY);
        if (file_fd < 0)
        {
            char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client_fd, response, strlen(response), 0);
        }
        else
        {
            char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
            send(client_fd, header, strlen(header), 0);
            if (strcmp(method, "GET") == 0)
            {
                // Send file content for GET requests only
                sendfile(client_fd, file_fd, NULL, BUFFER_SIZE);
            }
            close(file_fd);
        }
    }
    else if (strcmp(method, "POST") == 0)
    {
        // Locate the end of headers
        char *header_end = strstr(buffer, "\r\n\r\n");
        if (header_end == NULL)
        {
            // Malformed request
            return;
        }
        header_end += 4; // Move past the "\r\n\r\n"

        // Calculate the length of headers
        int headers_length = header_end - buffer;
        int body_length = bytes_read - headers_length; // Initial body length

        // Find Content-Length header
        int contentLength = 0;
        char *contentLengthStr = strstr(buffer, "Content-Length: ");
        if (contentLengthStr)
        {
            sscanf(contentLengthStr, "Content-Length: %d", &contentLength);
        }

        // Read more data if the entire body has not been received yet
        if (body_length < contentLength)
        {
            int additionalBytesRead = recv(client_fd, header_end + body_length, contentLength - body_length, 0);
            if (additionalBytesRead < 0)
            {
                perror("Failed to read the complete body");
                return;
            }
        }

        // Process POST body
        char *body = header_end;
        body[contentLength] = '\0'; // Null-terminate the body
        printf("POST Data: %s\n", body);

        // Send response back to client
        char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
    }
    else if (strcmp(method, "PUT") == 0)
    {
        // Similar logic to POST for reading headers and body
        char *header_end = strstr(buffer, "\r\n\r\n");
        if (header_end == NULL)
        {
            // Malformed request
            return;
        }
        header_end += 4; // Move past the "\r\n\r\n"

        int headers_length = header_end - buffer;
        int body_length = bytes_read - headers_length;

        int contentLength = 0;
        char *contentLengthStr = strstr(buffer, "Content-Length: ");
        if (contentLengthStr)
        {
            sscanf(contentLengthStr, "Content-Length: %d", &contentLength);
        }

        if (body_length < contentLength)
        {
            int additionalBytesRead = recv(client_fd, header_end + body_length, contentLength - body_length, 0);
            if (additionalBytesRead < 0)
            {
                perror("Failed to read the complete body");
                return;
            }
        }

        // Process and save PUT data (typically to a file)
        char *body = header_end;
        body[contentLength] = '\0';
        printf("PUT Data: %s\n", body);

        // Saving data logic goes here
        // For example, save to a file named after the path

        char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
    }

    else
    {
        // Method not implemented
        char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
    }

    close(client_fd);
    return NULL;
}
