#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define BUFFER_SIZE 4096
#define TEST_PORT 8087
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define RESET "\033[0m"

pid_t server_pid = -1;
int passed_tests = 0;
int total_tests = 0;

// Kill any existing process using the test port
void kill_existing_process() {
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "fuser -k %d/tcp >/dev/null 2>&1", TEST_PORT);
    system(cmd);
    sleep(1);
}

void setup_test_environment() {
    // Clean up any previous test files
    system("rm -rf test_files");
    system("mkdir -p test_files");

    // Create test.html
    FILE *f = fopen("test_files/test.html", "w");
    if (f) {
        fprintf(f, "<html><body>Test</body></html>");
        fclose(f);
    }

    // Create test.txt
    f = fopen("test_files/test.txt", "w");
    if (f) {
        fprintf(f, "Test content");
        fclose(f);
    }

    // Create forbidden.txt
    f = fopen("test_files/forbidden.txt", "w");
    if (f) {
        fprintf(f, "Forbidden content");
        fclose(f);
        chmod("test_files/forbidden.txt", 0000);
    }

    // Create index.html
    f = fopen("test_files/index.html", "w");
    if (f) {
        fprintf(f, "<html><body>Index</body></html>");
        fclose(f);
    }

    // Create large binary file (10MB)
    f = fopen("test_files/large.bin", "wb");
    if (f) {
        char buffer[1024];
        memset(buffer, 'A', sizeof(buffer));
        for (int i = 0; i < 10240; i++) {
            fwrite(buffer, 1, sizeof(buffer), f);
        }
        fclose(f);
    }

    // Create test files with different extensions
    // MP3
    f = fopen("test_files/test.mp3", "wb");
    if (f) {
        fwrite("MP3DATA", 1, 7, f);
        fclose(f);
    }

    // MP4
    f = fopen("test_files/test.mp4", "wb");
    if (f) {
        fwrite("MP4DATA", 1, 7, f);
        fclose(f);
    }

    // PNG
    f = fopen("test_files/test.png", "wb");
    if (f) {
        fwrite("PNGDATA", 1, 7, f);
        fclose(f);
        // Create test files with different extensions
        const char *test_files[] = {
                "test.html", "test.jpg", "test.gif", "test.png",
                "test.css", "test.au", "test.wav", "test.avi",
                "test.mpeg", "test.mp3"
        };

        for (int i = 0; i < sizeof(test_files) / sizeof(test_files[0]); i++) {
            char path[256];
            snprintf(path, sizeof(path), "test_files/%s", test_files[i]);
            FILE *f = fopen(path, "w");
            if (f) {
                fprintf(f, "TEST DATA for %s", test_files[i]);
                fclose(f);
            }
        }
    }
}

void cleanup_test_environment() {
    system("rm -rf test_files");
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        waitpid(server_pid, NULL, 0);
    }
}

void handle_exit() {
    cleanup_test_environment();
    exit(1);
}

void start_server() {
    kill_existing_process();

    server_pid = fork();
    if (server_pid == 0) {
        char port_str[10];
        snprintf(port_str, sizeof(port_str), "%d", TEST_PORT);
        execl("./server", "./server", port_str, "4", "8", "100", NULL);
        perror("Failed to start server");
        exit(1);
    }
    sleep(2);
}

int send_request(const char* method, const char* path, char* response) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "%s %s HTTP/1.0\r\nHost: localhost\r\n\r\n", method, path);
    write(sock, request, strlen(request));

    int total_read = 0;
    int bytes_read;
    while ((bytes_read = read(sock, response + total_read, BUFFER_SIZE - total_read - 1)) > 0) {
        total_read += bytes_read;
    }
    response[total_read] = '\0';

    close(sock);
    return total_read;
}

void print_test_result(const char* test_name, int passed, const char* response) {
    total_tests++;
    if (passed) {
        passed_tests++;
        printf("%s✓ %s: PASSED%s\n", GREEN, test_name, RESET);
    } else {
        printf("%s✗ %s: FAILED%s\n", RED, test_name, RESET);
        printf("Response received:\n%s\n", response);
    }
}

void test_302_redirect() {
    char response[BUFFER_SIZE];
    send_request("GET", "/test_files", response);
    print_test_result("302 Redirect",
                      strstr(response, "HTTP/1.0 302 Found") != NULL &&
                      strstr(response, "Location: /test_files/") != NULL,
                      response);
}

void test_404_not_found() {
    char response[BUFFER_SIZE];
    send_request("GET", "/nonexistent.html", response);
    print_test_result("404 Not Found",
                      strstr(response, "HTTP/1.0 404 Not Found") != NULL,
                      response);
}

void test_403_forbidden() {
    char response[BUFFER_SIZE];
    send_request("GET", "/test_files/forbidden.txt", response);
    print_test_result("403 Forbidden",
                      strstr(response, "HTTP/1.0 403 Forbidden") != NULL,
                      response);
}

void test_501_not_implemented() {
    char response[BUFFER_SIZE];
    send_request("POST", "/test_files/", response);
    print_test_result("501 Not Implemented",
                      strstr(response, "HTTP/1.0 501 Not supported") != NULL,
                      response);
}

void test_400_bad_request() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    write(sock, "MALFORMED REQUEST\r\n\r\n", 21);

    char response[BUFFER_SIZE];
    int bytes_read = read(sock, response, BUFFER_SIZE - 1);
    response[bytes_read] = '\0';
    close(sock);

    print_test_result("400 Bad Request",
                      strstr(response, "HTTP/1.0 400 Bad Request") != NULL,
                      response);
}

void test_directory_listing() {
    unlink("test_files/index.html");

    char response[BUFFER_SIZE];
    send_request("GET", "/test_files/", response);
    print_test_result("Directory Listing",
                      strstr(response, "HTTP/1.0 200 OK") != NULL &&
                      strstr(response, "Index of") != NULL,
                      response);

    FILE* f = fopen("test_files/index.html", "w");
    if (f) {
        fprintf(f, "<html><body>Index</body></html>");
        fclose(f);
    }
}

void test_index_html() {
    char response[BUFFER_SIZE];
    send_request("GET", "/test_files/", response);
    print_test_result("Index.html",
                      strstr(response, "HTTP/1.0 200 OK") != NULL &&
                      strstr(response, "<body>Index</body>") != NULL,
                      response);
}

void test_mime_types() {
    char response[BUFFER_SIZE];

    // Test HTML mime types
    FILE* f = fopen("test_files/test.html", "w");
    if (f) { fprintf(f, "HTML TEST"); fclose(f); }
    send_request("GET", "/test_files/test.html", response);
    print_test_result("HTML MIME Type",
                      strstr(response, "Content-Type: text/html") != NULL,
                      response);

    // Test JPEG mime types
    f = fopen("test_files/test.jpg", "w");
    if (f) { fprintf(f, "JPG TEST"); fclose(f); }
    send_request("GET", "/test_files/test.jpg", response);
    print_test_result("JPEG MIME Type",
                      strstr(response, "Content-Type: image/jpeg") != NULL,
                      response);

    // Test GIF mime type
    f = fopen("test_files/test.gif", "w");
    if (f) { fprintf(f, "GIF TEST"); fclose(f); }
    send_request("GET", "/test_files/test.gif", response);
    print_test_result("GIF MIME Type",
                      strstr(response, "Content-Type: image/gif") != NULL,
                      response);

    // Test PNG mime type
    f = fopen("test_files/test.png", "w");
    if (f) { fprintf(f, "PNG TEST"); fclose(f); }
    send_request("GET", "/test_files/test.png", response);
    print_test_result("PNG MIME Type",
                      strstr(response, "Content-Type: image/png") != NULL,
                      response);

    // Test CSS mime type
    f = fopen("test_files/test.css", "w");
    if (f) { fprintf(f, "CSS TEST"); fclose(f); }
    send_request("GET", "/test_files/test.css", response);
    print_test_result("CSS MIME Type",
                      strstr(response, "Content-Type: text/css") != NULL,
                      response);

    // Test AU mime type
    f = fopen("test_files/test.au", "w");
    if (f) { fprintf(f, "AU TEST"); fclose(f); }
    send_request("GET", "/test_files/test.au", response);
    print_test_result("AU MIME Type",
                      strstr(response, "Content-Type: audio/basic") != NULL,
                      response);

    // Test WAV mime type
    f = fopen("test_files/test.wav", "w");
    if (f) { fprintf(f, "WAV TEST"); fclose(f); }
    send_request("GET", "/test_files/test.wav", response);
    print_test_result("WAV MIME Type",
                      strstr(response, "Content-Type: audio/wav") != NULL,
                      response);

    // Test AVI mime type
    f = fopen("test_files/test.avi", "w");
    if (f) { fprintf(f, "AVI TEST"); fclose(f); }
    send_request("GET", "/test_files/test.avi", response);
    print_test_result("AVI MIME Type",
                      strstr(response, "Content-Type: video/x-msvideo") != NULL,
                      response);

    // Test MPEG mime type
    f = fopen("test_files/test.mpeg", "w");
    if (f) { fprintf(f, "MPEG TEST"); fclose(f); }
    send_request("GET", "/test_files/test.mpeg", response);
    print_test_result("MPEG MIME Type",
                      strstr(response, "Content-Type: video/mpeg") != NULL,
                      response);

    // Test MP3 mime type
    f = fopen("test_files/test.mp3", "w");
    if (f) { fprintf(f, "MP3 TEST"); fclose(f); }
    send_request("GET", "/test_files/test.mp3", response);
    print_test_result("MP3 MIME Type",
                      strstr(response, "Content-Type: audio/mpeg") != NULL,
                      response);
}

void test_large_file_handling() {
    char response[BUFFER_SIZE];
    send_request("GET", "/test_files/large.bin", response);

    // Check headers
    int has_content_length = strstr(response, "Content-Length: 10485760") != NULL;
    int has_connection_close = strstr(response, "Connection: close") != NULL;
    int has_200_ok = strstr(response, "HTTP/1.0 200 OK") != NULL;

    print_test_result("Large File Handling",
                      has_content_length && has_connection_close && has_200_ok,
                      response);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_exit);
    signal(SIGTERM, handle_exit);

    printf("\n🚀 Starting HTTP Server Tests...\n\n");

    setup_test_environment();
    start_server();

    // Run all tests
    test_302_redirect();
    test_404_not_found();
    test_403_forbidden();
    test_501_not_implemented();
    test_400_bad_request();
    test_directory_listing();
    test_index_html();
    test_mime_types();
    test_large_file_handling();

    // Print summary
    printf("\n📊 Test Summary:\n");
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Total: %d\n", total_tests);
    printf("Success Rate: %.2f%%\n\n", (float)passed_tests/total_tests * 100);

    cleanup_test_environment();
    return (passed_tests == total_tests) ? 0 : 1;
}