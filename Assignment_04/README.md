# Multiplayer Number Guessing Game Server

A TCP server implementation that manages a multiplayer number guessing game using non-blocking I/O with select().

## Overview

This implementation provides a server that:
- Handles multiple concurrent TCP client connections
- Uses select() for non-blocking I/O operations
- Manages a number guessing game where players try to guess a randomly generated number
- Implements a message queue system for reliable message delivery to clients

## Code Structure

### Key Components

1. Data Structures:
```c
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
```

2. Core Functions:
- `handle_new_connection()`: Processes new client connections
- `handle_player_read()`: Handles player input/guesses
- `handle_player_write()`: Manages message delivery to players
- `handle_guess()`: Processes game logic for player guesses
- `add_message_to_all_except()`: Broadcasts messages to players
- `disconnect_player()`: Manages player disconnection

## Building and Running

Compile with:
```bash
gcc -o server gameServer.c
```

Run the server:
```bash
./server <port> <seed> <max-players>
```

Example:
```bash
./server 8080 12345 4
```

## Implementation Features

### Non-blocking I/O
- All sockets are set to non-blocking mode using `O_NONBLOCK`
- Single select() loop manages all I/O events
- Prevents any blocking operations that could stall the server

### Message Queue System
- Each player has a dedicated message queue
- Messages are buffered until they can be sent
- Write operations only occur when socket is ready
- Maximum of `MAX_MESSAGES` (10) queued messages per player

### Player Management
- Dynamic player ID assignment using available ID pool
- Proper socket cleanup on disconnection
- Automatic game reset when a player wins
- Maximum player limit enforcement

### Input Validation
- Command line argument validation
- Port number range checking (1-65536)
- Player count validation (must be > 1)
- Numeric input validation for all parameters

### Error Handling
- Graceful cleanup on SIGINT (Ctrl+C)
- System call error reporting via perror()
- Memory leak prevention with proper cleanup
- Socket error detection and handling

## Technical Details

### Constants
```c
#define MAX_BUFFER 1024     // Maximum message size
#define MAX_MESSAGES 10     // Maximum queued messages per player
#define TARGET_MAX 100      // Maximum random number range
```

### Memory Management
- Dynamic allocation for server and player structures
- Cleanup function ensures all resources are freed
- Socket descriptors properly closed on exit

### Socket Configuration
- TCP (SOCK_STREAM) sockets
- SO_REUSEADDR option enabled
- Non-blocking mode for all sockets
- Select timeout set to NULL (blocking select)

## Limitations

1. Fixed buffer sizes:
   - 1024 bytes maximum message size
   - 10 queued messages per player maximum

2. Game constraints:
   - Random number range: 1-100
   - No persistence between server restarts
   - Single game instance per server

## Future Improvements

Potential enhancements that could be made:
1. Configurable message queue size
2. Adjustable random number range
3. Multiple simultaneous games
4. Score tracking system
5. Timeout for inactive players
6. Configuration file support
