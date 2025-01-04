#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_URL_LENGTH 1024
#define MAX_HOST_LENGTH 256
#define MAX_PATH_LENGTH 512
#define MAX_REQUEST_LENGTH 2048
#define MAX_RESPONSE_LENGTH 65536
#define MAX_REDIRECTS 5

typedef struct {
    char host[MAX_HOST_LENGTH];
    char path[MAX_PATH_LENGTH];
    int port;
} URLComponents;

// Function to extract Location URL from response
int extract_location_url(char* response, char* location_url, URLComponents* current_components) {
    char* location_header = strstr(response, "\nLocation: ");
    if (!location_header) {
        location_header = strstr(response, "\r\nLocation: ");
    }
    if (!location_header) return 0;

    // Move past "Location: "
    location_header = strchr(location_header, ':') + 2;

    // Find end of line
    char* end_line = strstr(location_header, "\r\n");
    if (!end_line) {
        end_line = strchr(location_header, '\n');
    }
    if (end_line) *end_line = '\0';

    // Remove any leading/trailing whitespace
    while (*location_header == ' ') location_header++;
    char* end = location_header + strlen(location_header) - 1;
    while (end > location_header && (*end == ' ' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    // Check if it's a relative redirect (doesn't start with http://)
    if (strncmp(location_header, "http://", 7) != 0) {
        // If path starts with /, use it as is
        if (location_header[0] == '/') {
            if (current_components->port == 80) {
                sprintf(location_url, "http://%s%s",
                        current_components->host,
                        location_header);
            } else {
                sprintf(location_url, "http://%s:%d%s",
                        current_components->host,
                        current_components->port,
                        location_header);
            }
        } else {
            // Need to append to current path
            char* last_slash = strrchr(current_components->path, '/');
            if (last_slash != NULL) {
                // Cut off at the last slash and append new relative path
                *last_slash = '\0';
                if (current_components->port == 80) {
                    sprintf(location_url, "http://%s%s/%s",
                            current_components->host,
                            current_components->path,
                            location_header);
                } else {
                    sprintf(location_url, "http://%s:%d%s/%s",
                            current_components->host,
                            current_components->port,
                            current_components->path,
                            location_header);
                }
                // Restore the slash
                *last_slash = '/';
            } else {
                // No slash in current path, just append to root
                if (current_components->port == 80) {
                    sprintf(location_url, "http://%s/%s",
                            current_components->host,
                            location_header);
                } else {
                    sprintf(location_url, "http://%s:%d/%s",
                            current_components->host,
                            current_components->port,
                            location_header);
                }
            }
        }
    } else {
        // It's an absolute URL
        strcpy(location_url, location_header);
    }

    // If end_line was modified, restore it
    if (end_line) *end_line = '\r';
    return 1;
}

// Function to parse URL
int parse_url(char* url, URLComponents* components) {
    if (strncmp(url, "http://", 7) != 0) {
        fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
        return -1;
    }

    char* host_start = url + 7;
    char* path_start = strchr(host_start, '/');
    char* port_start = strchr(host_start, ':');

    // Set default port
    components->port = 80;

    // Handle port if specified
    if (port_start && (!path_start || port_start < path_start)) {
        *port_start = '\0';
        port_start++;

        // Extract port number
        char* endptr;
        long port = strtol(port_start, &endptr, 10);
        if (*endptr != '/' && *endptr != '\0') {
            fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
            return -1;
        }
        if (port <= 0 || port >= 65536) {
            fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
            return -1;
        }
        components->port = (int)port;

        // Update path_start if needed
        if (*endptr == '/') {
            path_start = endptr;
        }
    }

    // Copy host
    size_t host_len = (path_start ? path_start - host_start : strlen(host_start));
    if (host_len >= MAX_HOST_LENGTH) {
        fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
        return -1;
    }
    strncpy(components->host, host_start, host_len);
    components->host[host_len] = '\0';

    // Copy path
    if (path_start) {
        strncpy(components->path, path_start, MAX_PATH_LENGTH - 1);
        components->path[MAX_PATH_LENGTH - 1] = '\0';
    } else {
        strcpy(components->path, "/");
    }

    return 0;
}

// Function to construct HTTP request
void construct_request(char* request, URLComponents* components, int param_count, char** params) {
    char full_path[MAX_PATH_LENGTH];
    strcpy(full_path, components->path);

    // Add parameters if any
    if (param_count > 0) {
        strcat(full_path, "?");
        for (int i = 0; i < param_count; i++) {
            if (i > 0) strcat(full_path, "&");
            strcat(full_path, params[i]);
        }
    }

    // Construct the request with proper line endings
    snprintf(request, MAX_REQUEST_LENGTH,
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             full_path, components->host);
}

// Function to send HTTP request and handle response
int send_request(URLComponents* components, int param_count, char** params) {
    int redirect_count = 0;
    char current_url[MAX_URL_LENGTH];
    URLComponents current_components = *components;
    char response[MAX_RESPONSE_LENGTH];
    char request[MAX_REQUEST_LENGTH];
    char location_url[MAX_URL_LENGTH];

    while (1) {
        // Get host by name
        struct hostent* server = gethostbyname(current_components.host);
        if (!server) {
            herror("gethostbyname");
            return -1;
        }

        // Create socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return -1;
        }

        // Set up server address
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        server_addr.sin_port = htons(current_components.port);

        // Connect to server
        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect");
            close(sockfd);
            return -1;
        }

        // Construct and send request
        construct_request(request, &current_components, param_count, params);
        printf("HTTP request =\n%s\nLEN = %d\n", request, (int)strlen(request));

        // Send the request
        if (write(sockfd, request, strlen(request)) < 0) {
            perror("write");
            close(sockfd);
            return -1;
        }

        // Read the response
        memset(response, 0, MAX_RESPONSE_LENGTH);
        int total_bytes = 0;
        int bytes_received;

        while ((bytes_received = read(sockfd, response + total_bytes,
                                      MAX_RESPONSE_LENGTH - total_bytes - 1)) > 0) {
            total_bytes += bytes_received;
        }

        if (bytes_received < 0) {
            perror("read");
            close(sockfd);
            return -1;
        }

        response[total_bytes] = '\0';
        for (int i = 0; i < total_bytes; ++i) {
            putchar(response[i]);
        }
        printf("\nTotal received response bytes: %d\n", total_bytes);

        // Check for redirect
        if (strncmp(response + 9, "3", 1) == 0) {  // 3xx status code
            if (extract_location_url(response, location_url, &current_components)) {
                // Parse new URL
                if (parse_url(location_url, &current_components) == 0) {
                    redirect_count++;
                    close(sockfd);
                    continue;
                }
            }
        }

        close(sockfd);
        break;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    int param_count = 0;
    char** params = NULL;
    char* url = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
                return 1;
            }

            i++; // Move to n
            char* endptr;
            param_count = strtol(argv[i], &endptr, 10);
            if (*endptr != '\0' || param_count < 0) {
                fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
                return 1;
            }

            if (param_count > 0) {
                params = malloc(param_count * sizeof(char*));
                for (int j = 0; j < param_count; j++) {
                    if (++i >= argc) {
                        fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
                        free(params);
                        return 1;
                    }
                    if (strchr(argv[i], '=') == NULL) {
                        fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
                        free(params);
                        return 1;
                    }
                    params[j] = argv[i];
                }
            }
        } else {
            if (url != NULL) {
                fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
                if (params) free(params);
                return 1;
            }
            url = argv[i];
        }
    }

    if (url == NULL) {
        fprintf(stderr, "Usage: client [-r n < pr1=value1 pr2=value2 ...>] <URL>\n");
        if (params) free(params);
        return 1;
    }

    // Parse initial URL
    URLComponents components;
    if (parse_url(url, &components) != 0) {
        if (params) free(params);
        return 1;
    }

    // Send request and handle response
    int result = send_request(&components, param_count, params);

    if (params) free(params);
    return result;
}

