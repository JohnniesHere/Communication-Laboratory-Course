# HTTP Server Implementation

## Overview
This project implements a basic HTTP server with thread pool functionality. The server handles HTTP GET requests, supports various MIME types, and includes features like directory listing and error handling.

## Features
- Multi-threaded server using thread pool architecture
- Support for HTTP GET method
- Directory listing
- Index.html auto-detection
- Various HTTP response codes (200, 302, 400, 403, 404, 501)
- MIME type support for common file types
- Large file handling
- Basic security features (permission checking)

## Supported MIME Types
- HTML (.html, .htm) - text/html
- JPEG (.jpg, .jpeg) - image/jpeg
- GIF (.gif) - image/gif
- PNG (.png) - image/png
- CSS (.css) - text/css
- Audio (.au) - audio/basic
- WAV (.wav) - audio/wav
- AVI (.avi) - video/x-msvideo
- MPEG (.mpeg, .mpg) - video/mpeg
- MP3 (.mp3) - audio/mpeg

## Project Structure
- `server.c` - Main HTTP server implementation
- `threadpool.c` - Thread pool implementation
- `threadpool.h` - Thread pool header file
- `server_test.c` - Comprehensive test suite
- `test.c` - Thread pool tester

## Building the Project
```bash
# Compile server
gcc -o server server.c threadpool.c -lpthread

# Compile test suite
gcc -o server_test server_test.c

# Compile thread pool tester
gcc -o test test.c threadpool.c -lpthread
```

## Running the Server
```bash
./server <port> <pool-size> <max-queue-size> <max-number-of-request>

Example:
./server 8080 4 8 100
```

### Parameters
- `port`: Port number to listen on
- `pool-size`: Number of threads in thread pool
- `max-queue-size`: Maximum size of request queue
- `max-number-of-request`: Maximum number of requests before server shutdown

## Testing
### Running the Test Suite
```bash
./server_test
```
This will run comprehensive tests checking:
- HTTP response codes
- MIME type handling
- Directory listing
- Large file transfers
- Error conditions

### Testing Thread Pool
```bash
./test <number of threads> <max queue size> <num jobs>

Example:
./test 4 8 10
```

## HTTP Response Format
The server generates responses in the following format:
```
HTTP/1.0 <status_code> <status_text>
Server: webserver/1.0
Date: <current_date>
[Content-Type: <mime_type>]
Content-Length: <length>
[Last-Modified: <modification_date>]
Connection: close

<content>
```

## Error Handling
- 200 OK: Successful request
- 302 Found: Directory redirect (adding trailing slash)
- 400 Bad Request: Malformed HTTP request
- 403 Forbidden: Permission denied
- 404 Not Found: Resource not found
- 501 Not Implemented: Unsupported HTTP method

## Thread Pool Implementation
The thread pool features:
- Fixed number of worker threads
- Queue for pending requests
- Thread synchronization using mutex and condition variables
- Graceful shutdown mechanism

## Limitations
- Only supports HTTP/1.0
- Only handles GET requests
- No keep-alive connections
- Maximum request line length: 4000 bytes
- Maximum directory entry length: 500 bytes

## Safety Features
- Path traversal protection
- File permission checking
- Buffer overflow prevention
- Input validation
- Error handling for system calls

## Requirements
- POSIX-compliant system
- GCC compiler
- pthread library
- Standard C libraries

## Best Practices
- Always compile with -lpthread flag
- Run server with appropriate permissions
- Monitor server logs for errors
- Test thoroughly before deployment
- Set appropriate buffer sizes for your use case

## Common Issues and Solutions
1. "Address already in use":
   ```bash
   sudo fuser -k <port>/tcp
   ```

2. Permission denied:
   - Check file permissions
   - Run with appropriate user privileges

3. Thread pool exhaustion:
   - Increase pool size
   - Adjust queue size
   - Monitor resource usage

## Performance Considerations
- Buffer size affects memory usage and transfer speed
- Thread pool size impacts concurrency and resource usage
- Queue size affects request handling under load
- File system performance affects response time

## Security Notes
- No directory traversal allowed
- Checks file permissions
- Validates HTTP requests
- Sanitizes file paths

