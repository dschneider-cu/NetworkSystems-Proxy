#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include "socket.h"
#include "../utils.h"

#define PORT_HTTP  80
#define PORT_HTTPS 443
#define PORT_MAX   65535

/** Struct for an HTTP header */
typedef struct {
    char *name;
    char *value;
} header_t;

/** Struct for the first line of an HTTP request */
typedef struct {
    char *method;
    char *url;
    char *version;
} startline_t;

/**
 * Struct containing all information in an HTTP request: startline (method, url, version), and a
 * list of headers.
 * (request content, ie POST is not supported)
 */
typedef struct {
    int nlines;
    startline_t startline;
    linkedlist_t headers; // data will be a header_t*
    bool complete;        // Whether the request is done parsing

    // Private variables
    // counter for crlfs (used to detect when headers are done)
    int _crlf_cntr;
} request_t;

typedef struct {
    const char *host; // Domain name
    int host_len;
    const char *path; // May be empty string
    int path_len;
    int port;
} url_t;

typedef enum {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_HTTP_VERSION_NOT_SUPPORTED = 505
} httpcode_t;

/**
 * Parse one line of an HTTP request and update the `out` struct accoringly.
 * 
 * @param request Pointer to the request
 * @param line Line string. May have trailing whitespace.
 * @param line_len Length of the line string
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_request_line(request_t *request, char *line, int line_len);

/**
 * Initializes a new request_t.
 */
void init_request(request_t *request);

/**
 * Free memory inside a request_t. Does not free `r` itself if it was allocated with malloc.
 */
void free_request(request_t *request);

/**
 * Parse the start line (the first line) of an HTTP request.
 * Format: `"[method] [url] [version]"`
 * 
 * @param startline Result startline_t struct that will be filled in
 * @param line Line string. No trailing whitespace.
 * @param line_len Length of the line string
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_start_line(startline_t *startline, char *line, int line_len);

/**
 * Free memory inside a startline_t. Does not free `s` itself if it was allocated with malloc.
 */
void free_startline(startline_t *startline);

/**
 * Parse an HTTP header from a line string.
 * Format: `"[name]: [value]"`
 * 
 * @param header header_t struct that will be filled in
 * @param line Line string. No trailing whitespace.
 * @param line_len Length of the line string
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_header(header_t *header, char *line, int line_len);

/**
 * Returns the value of the given header name.
 * 
 * @param request Pointer to the request
 * @param name Name of the header to fetch
 * @return String value of the header, or NULL if the request doesn't have the header
 */
const char *http_get_header(const request_t *request, const char *name);

/**
 * Parse a URL into host, path, and port given a request. The strings in the resulting url struct
 * will point to indices in the `request->startline.url` string so they will *not* be null
 * terminated.
 * 
 * @param url Result URL struct that will be filled in
 * @param request HTTP request
 * @return An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_url(url_t *url, const request_t *request);

/**
 * Receives an HTTP request from a socket reader and outputs a parsed request.
 * 
 * @param reader Socket reader (must be initialized already)
 * @param out_sockcode Output parameter for the socket result code
 * @param out_request Output parameter for the parsed HTTP request
 * @return An HTTP status code representing the result of the parsing
 */
httpcode_t http_receive_request(sockreader_t *reader, int *out_sockcode, request_t *out_request);

/**
 * Free memory inside a header_t. Does not free `h` itself if it was allocated with malloc.
 */
void free_header(header_t *header);

/**
 * Create a request_t from strings.
 *
 * @param method HTTP method ("GET")
 * @param url Request url
 * @param version Version ("HTTP/1.0")
 * @param headers Array of headers; must be pairs of strings [name, value, name, value, ...]
 * @param nheaders Number of headers, should be length of `headers` / 2
 * @return An allocated request_t or NULL if parsing failed
 */
request_t *http_make_request(
    const char *method,
    const char *url,
    const char *version,
    const char **headers,
    int nheaders
);

/**
 * Make an HTTP response string given a status code, mimetype, and content. If mimetype or content
 * are NULL, they will not be included in the response.
 */
char* http_make_response(
    httpcode_t code,
    const char* mimetype,
    const unsigned char* content,
    int content_len
);

/**
 * Returns the string message associated with the given HTTP code, eg. "Not Found" for 404.
 */
const char *http_code_to_str(httpcode_t code);

void print_request(const request_t *request);
void print_url(const url_t *url);

#endif