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
#define TEST_PORT 8087  // Changed to less commonly used port
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
    sleep(1);  // Wait for port to be freed
}

void setup_test_environment() {
    // Clean up any previous test files
    system("rm -rf test_files");

    // Create test directory and files
    system("mkdir -p test_files");

    // Create test.html
    FILE* f = fopen("test_files/test.html", "w");
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
        // Child process - exec server
        char port_str[10];
        snprintf(port_str, sizeof(port_str), "%d", TEST_PORT);
        execl("./server", "./server", port_str, "4", "8", "100", NULL);
        perror("Failed to start server");
        exit(1);
    }
    sleep(2); // Wait longer for server to start
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
        printf("Response received:\n%s\n", response);  // Print response for failed tests
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
    // First remove index.html for this test
    unlink("test_files/index.html");  // Remove index.html

    char response[BUFFER_SIZE];
    send_request("GET", "/test_files/", response);
    print_test_result("Directory Listing",
                      strstr(response, "HTTP/1.0 200 OK") != NULL &&
                      strstr(response, "Index of") != NULL,
                      response);

    // Recreate index.html for other tests
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
    send_request("GET", "/test_files/test.html", response);
    print_test_result("MIME Types",
                      strstr(response, "Content-Type: text/html") != NULL,
                      response);
}

int main(int argc, char *argv[]) {
    // Set up signal handlers
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

    // Print summary
    printf("\n📊 Test Summary:\n");
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Total: %d\n", total_tests);
    printf("Success Rate: %.2f%%\n\n", (float)passed_tests/total_tests * 100);

    cleanup_test_environment();
    return (passed_tests == total_tests) ? 0 : 1;
}