#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "threadpool.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 4096

typedef struct {
    int client_fd;
    char* base_path;
} client_info;

char* get_mime_type(char* name) {
    char* ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

void send_error_response(int client_fd, int status_code, const char* status_text, const char* message) {
    char response[BUFFER_SIZE];
    char timebuf[128];
    time_t now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char html[512];
    snprintf(html, sizeof(html),
             "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n"
             "<BODY><H4>%d %s</H4>\r\n"
             "%s\r\n"
             "</BODY></HTML>\r\n",
             status_code, status_text, status_code, status_text, message);

    snprintf(response, sizeof(response),
             "HTTP/1.0 %d %s\r\n"
             "Server: webserver/1.0\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n%s",
             status_code, status_text, timebuf, strlen(html), html);

    write(client_fd, response, strlen(response));
}

void send_302_response(int client_fd, const char* path) {
    char timebuf[128];
    time_t now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    // Create HTML content first
    const char* html_template =
            "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n"
            "<BODY><H4>302 Found</H4>\r\n"
            "Directories must end with a slash.\r\n"
            "</BODY></HTML>\r\n";

    // Create location with strict size checking
    char location[MAX_PATH_LENGTH];
    size_t path_len = strlen(path);
    if (path_len >= MAX_PATH_LENGTH - 2) { // -2 for '/' and null terminator
        send_error_response(client_fd, 500, "Internal Server Error", "Path too long");
        return;
    }

    // Build location safely
    strncpy(location, path, MAX_PATH_LENGTH - 2);
    location[MAX_PATH_LENGTH - 2] = '\0';  // Ensure null termination
    strcat(location, "/");

    // First create the response headers
    char response[BUFFER_SIZE];
    int response_len = snprintf(response, sizeof(response),
                                "HTTP/1.0 302 Found\r\n"
                                "Server: webserver/1.0\r\n"
                                "Date: %s\r\n"
                                "Location: %s\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: %zu\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                timebuf,
                                location,
                                strlen(html_template));

    if (response_len >= BUFFER_SIZE) {
        send_error_response(client_fd, 500, "Internal Server Error", "Response too large");
        return;
    }

    // Write headers and body separately
    write(client_fd, response, strlen(response));
    write(client_fd, html_template, strlen(html_template));
}

void send_directory_content(int client_fd, const char* path, const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        send_error_response(client_fd, 500, "Internal Server Error", "Unable to read directory");
        return;
    }

    char* html_content = malloc(BUFFER_SIZE * 8);
    if (html_content == NULL) {
        closedir(dir);
        send_error_response(client_fd, 500, "Internal Server Error", "Memory allocation failed");
        return;
    }

    char* current_pos = html_content;
    current_pos += sprintf(current_pos,
                           "<HTML>\r\n"
                           "<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n"
                           "<BODY>\r\n"
                           "<H4>Index of %s</H4>\r\n"
                           "<table CELLSPACING=8>\r\n"
                           "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n",
                           path, path);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        char full_entry_path[MAX_PATH_LENGTH];
        snprintf(full_entry_path, sizeof(full_entry_path), "%s/%s", dir_path, entry->d_name);

        struct stat entry_stat;
        if (stat(full_entry_path, &entry_stat) == 0) {
            char timebuf[128];
            strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&entry_stat.st_mtime));

            current_pos += sprintf(current_pos,
                                   "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%s%ld</td></tr>\r\n",
                                   entry->d_name, entry->d_name, timebuf,
                                   S_ISREG(entry_stat.st_mode) ? "" : "-",
                                   S_ISREG(entry_stat.st_mode) ? entry_stat.st_size : 0);
        }
    }

    current_pos += sprintf(current_pos,
                           "</table>\r\n"
                           "<HR>\r\n"
                           "<ADDRESS>webserver/1.0</ADDRESS>\r\n"
                           "</BODY></HTML>\r\n");

    char headers[BUFFER_SIZE];
    char timebuf[128];
    time_t now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    int content_length = strlen(html_content);
    snprintf(headers, sizeof(headers),
             "HTTP/1.0 200 OK\r\n"
             "Server: webserver/1.0\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             timebuf, content_length);

    write(client_fd, headers, strlen(headers));
    write(client_fd, html_content, content_length);

    free(html_content);
    closedir(dir);
}

void send_file_content(int client_fd, const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (file == NULL) {
        send_error_response(client_fd, 500, "Internal Server Error", "Unable to read file");
        return;
    }

    struct stat file_stat;
    if (stat(filepath, &file_stat) < 0) {
        fclose(file);
        send_error_response(client_fd, 500, "Internal Server Error", "Unable to get file info");
        return;
    }

    char headers[BUFFER_SIZE];
    char timebuf[128];
    char mod_timebuf[128];
    time_t now = time(NULL);

    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    strftime(mod_timebuf, sizeof(mod_timebuf), RFC1123FMT, gmtime(&file_stat.st_mtime));

    char* mime_type = get_mime_type((char*)filepath);
    if (mime_type != NULL) {
        snprintf(headers, sizeof(headers),
                 "HTTP/1.0 200 OK\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %ld\r\n"
                 "Last-Modified: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 timebuf, mime_type, file_stat.st_size, mod_timebuf);
    } else {
        snprintf(headers, sizeof(headers),
                 "HTTP/1.0 200 OK\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Length: %ld\r\n"
                 "Last-Modified: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 timebuf, file_stat.st_size, mod_timebuf);
    }

    write(client_fd, headers, strlen(headers));

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        write(client_fd, buffer, bytes_read);
    }

    fclose(file);
}

int handle_client(void* arg) {
    client_info* info = (client_info*)arg;
    char buffer[BUFFER_SIZE];
    char method[16], path[MAX_PATH_LENGTH], protocol[16];

    ssize_t bytes_read = read(info->client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        goto cleanup;
    }
    buffer[bytes_read] = '\0';

    if (sscanf(buffer, "%15s %2047s %15s", method, path, protocol) != 3) {
        send_error_response(info->client_fd, 400, "Bad Request", "Bad Request.");
        goto cleanup;
    }

    if (strncmp(protocol, "HTTP/", 5) != 0) {
        send_error_response(info->client_fd, 400, "Bad Request", "Bad Request.");
        goto cleanup;
    }

    if (strcmp(method, "GET") != 0) {
        send_error_response(info->client_fd, 501, "Not supported", "Method is not supported.");
        goto cleanup;
    }

    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s%s", info->base_path, path);

    struct stat path_stat;
    if (stat(full_path, &path_stat) < 0) {
        send_error_response(info->client_fd, 404, "Not Found", "File not found.");
        goto cleanup;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        if (path[strlen(path) - 1] != '/') {
            send_302_response(info->client_fd, path);
            goto cleanup;
        }

// In handle_client function, replace the index_path construction with:
        char index_path[MAX_PATH_LENGTH];
        size_t full_path_len = strlen(full_path);
        if (full_path_len >= MAX_PATH_LENGTH - 12) { // -12 for "/index.html" and null terminator
            send_error_response(info->client_fd, 500, "Internal Server Error", "Path too long");
            goto cleanup;
        }
        strcpy(index_path, full_path);
        strcat(index_path, "/index.html");

        if (access(index_path, R_OK) == 0) {
            send_file_content(info->client_fd, index_path);
        } else {
            send_directory_content(info->client_fd, path, full_path);
        }
    } else if (S_ISREG(path_stat.st_mode)) {
        if (access(full_path, R_OK) != 0) {
            send_error_response(info->client_fd, 403, "Forbidden", "Access denied.");
        } else {
            send_file_content(info->client_fd, full_path);
        }
    } else {
        send_error_response(info->client_fd, 403, "Forbidden", "Access denied.");
    }

    cleanup:
    close(info->client_fd);
    free(info->base_path);
    free(info);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: server <port> <pool-size> <max-queue-size> <max-number-of-request>\n");
        exit(1);
    }

    int port = atoi(argv[1]);
    int pool_size = atoi(argv[2]);
    int max_queue_size = atoi(argv[3]);
    int max_requests = atoi(argv[4]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }

    threadpool* pool = create_threadpool(pool_size, max_queue_size);
    if (pool == NULL) {
        perror("create_threadpool");
        exit(1);
    }

    int request_count = 0;
    while (request_count < max_requests) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        client_info* info = malloc(sizeof(client_info));
        if (info == NULL) {
            close(client_fd);
            continue;
        }
        info->client_fd = client_fd;
        info->base_path = getcwd(NULL, 0);

        dispatch(pool, handle_client, info);
        request_count++;
    }

    destroy_threadpool(pool);
    close(server_fd);
    return 0;
}