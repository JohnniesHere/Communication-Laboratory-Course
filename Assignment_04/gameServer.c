#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_BUFFER 1024
#define MAX_MESSAGES 10
#define TARGET_MAX 100

typedef struct {
    int socket_fd;
    int id;
    bool active;
    char write_buffer[MAX_MESSAGES][MAX_BUFFER];
    int write_count;
    int write_index;
} Player;

typedef struct {
    Player* players;
    int max_players;
    int target_number;
    int welcome_socket;
    bool running;
    fd_set read_fds;
    fd_set write_fds;
    int max_fd;
} GameServer;

GameServer* server = NULL;

void cleanup() {
    if (server) {
        if (server->welcome_socket >= 0)
            close(server->welcome_socket);

        if (server->players) {
            for (int i = 0; i < server->max_players; i++) {
                if (server->players[i].socket_fd >= 0)
                    close(server->players[i].socket_fd);
            }
            free(server->players);
        }
        free(server);
    }
}

void handle_signal(int signum) {
    (void)signum;  // Mark parameter as unused
    server->running = false;
}

void add_message_to_all_except(const char* message, int except_id) {
    for (int i = 0; i < server->max_players; i++) {
        if (server->players[i].active && server->players[i].id != except_id) {
            strcpy(server->players[i].write_buffer[server->players[i].write_count], message);
            server->players[i].write_count++;
        }
    }
}

void add_message_to_player(const char* message, int player_id) {
    for (int i = 0; i < server->max_players; i++) {
        if (server->players[i].active && server->players[i].id == player_id) {
            strcpy(server->players[i].write_buffer[server->players[i].write_count], message);
            server->players[i].write_count++;
            break;
        }
    }
}

void generate_new_target() {
    server->target_number = (rand() % TARGET_MAX) + 1;
}

int find_available_id() {
    for (int i = 0; i < server->max_players; i++) {
        if (!server->players[i].active)
            return i + 1;
    }
    return -1;
}

void disconnect_player(int id) {
    for (int i = 0; i < server->max_players; i++) {
        if (server->players[i].active && server->players[i].id == id) {
            close(server->players[i].socket_fd);
            server->players[i].active = false;
            server->players[i].socket_fd = -1;

            char message[MAX_BUFFER];
            snprintf(message, sizeof(message), "Player %d disconnected\n", id);
            add_message_to_all_except(message, id);
            break;
        }
    }
}

void handle_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // First check if we have room for new players
    int active_players = 0;
    for (int i = 0; i < server->max_players; i++) {
        if (server->players[i].active) {
            active_players++;
        }
    }

    if (active_players >= server->max_players) {
        // Don't accept() the connection if we're full - let it stay in listen backlog
        return;
    }

    printf("Server is ready to read from welcome socket %d\n", server->welcome_socket);

    int client_fd = accept(server->welcome_socket, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
        return;
    }

    // Rest of the connection handling remains the same...
    int new_id = find_available_id();
    if (new_id == -1) {
        close(client_fd);
        return;
    }

    // Set non-blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // Initialize player
    for (int i = 0; i < server->max_players; i++) {
        if (!server->players[i].active) {
            server->players[i].socket_fd = client_fd;
            server->players[i].id = new_id;
            server->players[i].active = true;
            server->players[i].write_count = 0;
            server->players[i].write_index = 0;

            char welcome[MAX_BUFFER];
            snprintf(welcome, sizeof(welcome), "Welcome to the game, your id is %d\n", new_id);
            add_message_to_player(welcome, new_id);

            char joined[MAX_BUFFER];
            snprintf(joined, sizeof(joined), "Player %d joined the game\n", new_id);
            add_message_to_all_except(joined, new_id);
            break;
        }
    }
}

void handle_guess(int player_id, int guess) {
    char message[MAX_BUFFER];
    snprintf(message, sizeof(message), "Player %d guessed %d\n", player_id, guess);
    add_message_to_all_except(message, -1);

    if (guess == server->target_number) {
        // Send win messages to all players
        snprintf(message, sizeof(message), "Player %d wins\n", player_id);
        add_message_to_all_except(message, -1);

        snprintf(message, sizeof(message), "The correct guessing is %d\n", guess);
        add_message_to_all_except(message, -1);

        // Immediately disconnect all players after adding messages
        for (int i = 0; i < server->max_players; i++) {
            if (server->players[i].active) {
                close(server->players[i].socket_fd);
                server->players[i].active = false;
                server->players[i].socket_fd = -1;
            }
        }

        generate_new_target();
    } else {
        snprintf(message, sizeof(message), "The guess %d is too %s\n",
                 guess, (guess > server->target_number) ? "high" : "low");
        add_message_to_all_except(message, -1);
    }
}

void handle_player_read(int player_id) {
    Player* player = NULL;
    for (int i = 0; i < server->max_players; i++) {
        if (server->players[i].active && server->players[i].id == player_id) {
            player = &server->players[i];
            break;
        }
    }

    if (!player)
        return;

    printf("Server is ready to read from player %d on socket %d\n",
           player_id, player->socket_fd);

    char buffer[MAX_BUFFER];
    int bytes = read(player->socket_fd, buffer, sizeof(buffer) - 1);

    if (bytes <= 0) {
        if (bytes == 0 || errno != EAGAIN) {
            disconnect_player(player_id);
        }
        return;
    }

    buffer[bytes] = '\0';
    int guess = atoi(buffer);
    if (guess > 0) {
        handle_guess(player_id, guess);
    }
}

void handle_player_write(int player_id) {
    Player* player = NULL;
    for (int i = 0; i < server->max_players; i++) {
        if (server->players[i].active && server->players[i].id == player_id) {
            player = &server->players[i];
            break;
        }
    }

    if (!player || player->write_index >= player->write_count)
        return;

    printf("Server is ready to write to player %d on socket %d\n",
           player_id, player->socket_fd);

    const char* message = player->write_buffer[player->write_index];
    int bytes = write(player->socket_fd, message, strlen(message));

    if (bytes < 0 && errno != EAGAIN) {
        disconnect_player(player_id);
        return;
    }

    if (bytes > 0) {
        player->write_index++;
        if (player->write_index >= player->write_count) {
            player->write_count = 0;
            player->write_index = 0;
        }
    }
}

bool validate_arguments(int argc, char* argv[], int* port, int* seed, int* max_players) {
    if (argc != 4) {
        return false;
    }

    // Validate all arguments are numbers
    for (int i = 1; i < argc; i++) {
        for (int j = 0; argv[i][j]; j++) {
            if (!isdigit(argv[i][j])) {
                return false;
            }
        }
    }

    *port = atoi(argv[1]);
    *seed = atoi(argv[2]);
    *max_players = atoi(argv[3]);

    if (*port <= 0 || *port > 65536 || *max_players <= 1) {
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    int port, seed, max_players;

    if (!validate_arguments(argc, argv, &port, &seed, &max_players)) {
        fprintf(stderr, "Usage: ./server <port> <seed> <max-number-of-players>\n");
        return 1;
    }

    server = malloc(sizeof(GameServer));
    if (!server) {
        perror("malloc failed");
        return 1;
    }

    server->players = malloc(max_players * sizeof(Player));
    if (!server->players) {
        perror("malloc failed");
        cleanup();
        return 1;
    }

    // Initialize server
    server->max_players = max_players;
    server->running = true;
    server->welcome_socket = -1;

    for (int i = 0; i < max_players; i++) {
        server->players[i].socket_fd = -1;
        server->players[i].active = false;
        server->players[i].write_count = 0;
        server->players[i].write_index = 0;
    }

    // Set up signal handling
    signal(SIGINT, handle_signal);

    // Initialize random number generator
    srand(seed);
    generate_new_target();

    // Create welcome socket
    server->welcome_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->welcome_socket < 0) {
        perror("socket failed");
        cleanup();
        return 1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server->welcome_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        cleanup();
        return 1;
    }

    // Bind welcome socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server->welcome_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        cleanup();
        return 1;
    }

    // Listen for connections
    if (listen(server->welcome_socket, max_players * 2) < 0) {
        perror("listen failed");
        cleanup();
        return 1;
    }

    // Set welcome socket to non-blocking
    int flags = fcntl(server->welcome_socket, F_GETFL, 0);
    fcntl(server->welcome_socket, F_SETFL, flags | O_NONBLOCK);

    // Main event loop
    while (server->running) {
        FD_ZERO(&server->read_fds);
        FD_ZERO(&server->write_fds);

        // Add welcome socket to read set
        FD_SET(server->welcome_socket, &server->read_fds);
        server->max_fd = server->welcome_socket;

        // Add active players to appropriate sets
        for (int i = 0; i < server->max_players; i++) {
            if (server->players[i].active) {
                FD_SET(server->players[i].socket_fd, &server->read_fds);
                if (server->players[i].write_count > 0) {
                    FD_SET(server->players[i].socket_fd, &server->write_fds);
                }
                if (server->players[i].socket_fd > server->max_fd) {
                    server->max_fd = server->players[i].socket_fd;
                }
            }
        }

        // Wait for activity
        if (select(server->max_fd + 1, &server->read_fds, &server->write_fds, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("select failed");
            break;
        }

        // Check welcome socket
        if (FD_ISSET(server->welcome_socket, &server->read_fds)) {
            handle_new_connection();
        }

        // Check player sockets
        for (int i = 0; i < server->max_players; i++) {
            if (!server->players[i].active)
                continue;

            if (FD_ISSET(server->players[i].socket_fd, &server->read_fds)) {
                handle_player_read(server->players[i].id);
            }

            if (FD_ISSET(server->players[i].socket_fd, &server->write_fds)) {
                handle_player_write(server->players[i].id);
            }
        }
    }

    cleanup();
    return 0;
}