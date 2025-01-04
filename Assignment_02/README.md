# HTTP Client

A simple HTTP/1.1 client implementation in C that supports URL parsing, GET requests with parameters, and HTTP redirects.

## Features

- HTTP/1.1 GET request support
- URL parsing with port number handling
- Query parameter support
- Handles both absolute and relative HTTP redirects
- IPv4 support
- Proper error handling
- Connection closing after each request

## Building

To compile the client, use gcc:

```bash
gcc -o client client.c
```

## Usage

The basic syntax is:
```bash
./client [-r n <pr1=value1 pr2=value2 ...>] <URL>
```

Where:
- `-r`: Optional flag to specify query parameters
- `n`: Number of parameters to follow
- `pr1=value1`: Parameter name-value pairs
- `<URL>`: The target URL (must start with http://)

### Examples

1. Basic GET request:
```bash
./client http://httpbin.org/get
```

2. Request with port number:
```bash
./client http://example.com:8080/path
```

3. Request with parameters:
```bash
./client -r 2 name=john age=25 http://httpbin.org/get
```

4. Testing redirects:
```bash
./client http://httpbin.org/redirect/2
./client http://httpbin.org/relative-redirect/1
```

## Technical Details

### URL Format
- Must start with "http://"
- Format: http://hostname[:port]/filepath
- Default port is 80 if not specified
- Port must be between 1 and 65535

### Request Format
The client constructs HTTP/1.1 requests with:
- GET method
- Host header
- Connection: close header
- Parameters added to URL path if specified

### Parameter Handling
- Parameters are added to the URL path with '?' prefix
- Multiple parameters are joined with '&'
- Each parameter must be in format: name=value

### Redirect Handling
- Supports 3XX redirect responses
- Handles both absolute and relative redirect URLs
- Maintains port information through redirects
- Relative paths are properly resolved

## Error Handling

The client handles various error conditions:
- Invalid URL format
- Invalid port numbers
- Network connection failures
- Host resolution failures
- Invalid parameter format
- Memory allocation failures

Error messages are displayed to stderr, and the program exits with status code 1 on error.

## Limitations

- Only supports HTTP (not HTTPS)
- Only supports GET requests
- Only supports IPv4
- Maximum response size is 65536 bytes
- Maximum URL length is 1024 characters
- Maximum host length is 256 characters
- Maximum path length is 512 characters

## Output Format

The client outputs:
1. The constructed HTTP request and its length
2. The complete server response
3. Total number of bytes received

## Example Output

```
HTTP request =
GET /get HTTP/1.1
Host: httpbin.org
Connection: close

LEN = 55

[Server response content here]

Total received response bytes: 515
```