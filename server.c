#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

const char *get_mime_type(const char *path);
void serve_static_file(int client_fd, const char *path);
void execute_php_script(int client_fd, const char *path);
void handle_post_put_request(int client_fd, const char *method, char *buffer, int buffer_length);
void *handle_client_request(void *client_fd_ptr);

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port 8080...\n");

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, (socklen_t *)&addr_len);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        if (client_fd_ptr == NULL)
        {
            perror("Memory allocation for client_fd_ptr failed");
            close(client_fd);
            continue;
        }

        *client_fd_ptr = client_fd;
        if (pthread_create(&thread_id, NULL, handle_client_request, client_fd_ptr) != 0)
        {
            perror("Failed to create thread");
            close(client_fd);
            free(client_fd_ptr);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}

const char *get_mime_type(const char *path)
{
    // This function returns the MIME type for given file path
    // Implement logic to determine MIME type based on file extension
    // For simplicity, here we only handle HTML and default to text/plain
    const char *ext = strrchr(path, '.');
    if (ext != NULL)
    {
        if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        {
            return "text/html";
        }
    }
    return "text/plain";
}

void serve_static_file(int client_fd, const char *path)
{
    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0)
    {
        perror("Error opening file");
        const char *notFoundResponse = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, notFoundResponse, strlen(notFoundResponse), 0);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0)
    {
        perror("Error getting file stats");
        close(file_fd);
        return;
    }

    const char *mime_type = get_mime_type(path);
    char header[512];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
             mime_type, file_stat.st_size);
    send(client_fd, header, strlen(header), 0);

    sendfile(client_fd, file_fd, NULL, file_stat.st_size);
    close(file_fd);
}

void execute_php_script(int client_fd, const char *path)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return;
    }
    else if (pid == 0)
    {
        // Child process
        close(pipefd[0]);                 // Close unused read end
        dup2(pipefd[1], STDOUT_FILENO);   // Redirect stdout to pipe
        execlp("php", "php", path, NULL); // Execute PHP script
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        close(pipefd[1]);      // Close unused write end
        waitpid(pid, NULL, 0); // Wait for child process to finish

        // Read output from PHP script
        char script_output[BUFFER_SIZE];
        read(pipefd[0], script_output, BUFFER_SIZE);
        close(pipefd[0]);

        // Send output as HTTP response
        char response_header[BUFFER_SIZE];
        snprintf(response_header, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        send(client_fd, response_header, strlen(response_header), 0);
        send(client_fd, script_output, strlen(script_output), 0);
    }
}

void handle_post_request(int client_fd, char *body) {
    FILE *file = fopen("post_data.txt", "a");  // Open in append mode
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    fprintf(file, "%s\n", body);  // Append the POST data to the file
    fclose(file);

    printf("POST Request Body: %s\n", body);
}
void handle_put_request(int client_fd, char *body) {
    FILE *file = fopen("put_data.txt", "w");  // Open in write mode (overwrite)
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    fprintf(file, "%s", body);  // Write the PUT data to the file
    fclose(file);

    printf("PUT Request Body: %s\n", body);
}

void handle_post_put_request(int client_fd, const char *method, char *buffer, int buffer_length)
{
    char *header_end = strstr(buffer, "\r\n\r\n");
    if (header_end == NULL)
    {
        const char *badRequestResponse = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, badRequestResponse, strlen(badRequestResponse), 0);
        return;
    }
    header_end += 4; // Skip past the header end marker

    int contentLength = 0;
    char *contentLengthStr = strstr(buffer, "Content-Length: ");
    if (contentLengthStr)
    {
        sscanf(contentLengthStr, "Content-Length: %d", &contentLength);
    }

    // Calculate the initial part of the body already read
    int initialBodyLength = buffer_length - (header_end - buffer);
    char *body = malloc(contentLength + 1); // +1 for null-terminator
    if (!body)
    {
        perror("Failed to allocate memory for body");
        return;
    }

    memcpy(body, header_end, initialBodyLength);
    int totalBytesRead = initialBodyLength;

    while (totalBytesRead < contentLength)
    {
        int bytesRead = recv(client_fd, body + totalBytesRead, contentLength - totalBytesRead, 0);
        if (bytesRead <= 0)
        {
            perror("Error reading request body");
            free(body);
            return;
        }
        totalBytesRead += bytesRead;
    }

    if (strcmp(method, "POST") == 0) {
        handle_post_request(client_fd, body);
    } else if (strcmp(method, "PUT") == 0) {
        handle_put_request(client_fd, body);
    }

    free(body);

    const char *okResponse = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    send(client_fd, okResponse, strlen(okResponse), 0);
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
        if (strstr(path, ".php") != NULL)
        {
            execute_php_script(client_fd, path + 1); // PHP handling
        }
        else
        {
            serve_static_file(client_fd, path + 1); // Static file serving
        }
    }
    else if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0)
    {
        handle_post_put_request(client_fd, method, buffer, bytes_read);
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
