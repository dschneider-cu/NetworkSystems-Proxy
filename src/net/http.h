#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include "socket.h"
#include "../utils.h"

#define PORT_HTTP  80
#define PORT_HTTPS 443
#define PORT_MAX   65535

typedef enum {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_HTTP_VERSION_NOT_SUPPORTED = 505
} httpcode_t;

/** Struct for an HTTP header */
typedef struct {
    char *name;
    char *value;
} header_t;

/**
 * Struct containing all information in an HTTP request: startline (method, url, version), and a
 * list of headers.
 * (request content, ie POST is not supported)
 */
typedef struct {
    int nlines;
    char *method;
    char *url;
    char *version;
    linkedlist_t headers; // data will be a header_t*
    bool complete;        // Whether the request is done parsing
} request_t;

typedef struct {
    int nlines;
    httpcode_t respcode;
    linkedlist_t headers; // data will be header_t*
    const char *content;
    int content_len;
    bool complete;        // Whether the response is done parsing
} response_t;

typedef struct {
    const char *host; // Domain name
    int host_len;
    const char *path; // May be empty string
    int path_len;
    int port;
} url_t;

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
 * Initializes a new response_t.
 */
void init_response(response_t *response);

/**
 * Parse the start line (the first line) of an HTTP request.
 * Format: `"[method] [url] [version]"`
 * 
 * @param request Request struct
 * @param line Line string. No trailing whitespace.
 * @param line_len Length of the line string
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_start_line(request_t *request, char *line, int line_len);

/**
 * Parse the status line (the first line) of an HTTP response.
 * Format: `"[version] [code] [text]"`
 * 
 * Version and text are ignored.
 * 
 * @param response Response struct
 * @param line Line string. No leading whitespace.
 * @param line_len Length of the line string
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_status_line(response_t *response, char *line, int line_len);

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
const char *http_get_request_header(const request_t *request, const char *name);

/**
 * Returns the value of the given header name.
 * 
 * @param response Pointer to the response
 * @param name Name of the header to fetch
 * @return String value of the header, or NULL if the request doesn't have the header
 */
const char *http_get_response_header(const response_t *response, const char *name);

/**
 * Parse a URL into host, path, and port given a request. The strings in the resulting url struct
 * will point to indices in the `request->url` string so they will *not* be null
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
httpcode_t http_receive_request(
    sockreader_t *reader,
    int *out_sockcode,
    request_t *out_request
);

/** Callback for processing received response content */
typedef httpcode_t (*resp_content_callback_t)(response_t *response, char *chunk, int chunk_len);

/**
 * Receives an HTTP response from a socket reader and outputs a parsed response.
 * 
 * @param reader Socket reader (must be initialized already)
 * @param out_sockcode Output parameter for the socket result code
 * @param out_response Output parameter for the parsed HTTP response
 * @param content_callback Callback function to process chunks of the received content. Return 
 *      value other than HTTP_OK will cancel the receive.
 * @return An HTTP status code representing the result of the parsing
 */
httpcode_t http_receive_response(
    sockreader_t *reader,
    int *out_sockcode,
    response_t *out_response,
    resp_content_callback_t content_callback
);

/**
 * Create a request_t from strings.
 *
 * @param method HTTP method ("GET")
 * @param url Request url
 * @param version Version ("HTTP/1.0")
 * @param headers Array of headers; must be pairs of strings [name, value, name, value, ...]
 * @param nheaders Number of headers, should be length of `headers` / 2
 * @return A heap allocated request_t or NULL if error
 */
request_t *http_make_request(
    const char *method,
    const char *url,
    const char *version,
    const char **headers,
    int nheaders
);

/**
 * Create a response_t
 * 
 * @param code HTTP response code
 * @param headers Array of headers; must be pairs of strings [name, value, name, value, ...]
 * @param nheaders Number of headers, should be length of `headers` / 2
 * @return A heap allocated response_t or NULL if error
 */
response_t* http_make_response(
    httpcode_t code,
    const char **headers,
    int nheaders,
    const char *content,
    int content_len
);

/**
 * Free memory inside a header_t. Does not free `header` itself if it was allocated with malloc.
 */
void free_header(header_t *header);

/**
 * Free memory inside a request_t. Does not free `request` itself if it was allocated with malloc.
 */
void free_request(request_t *request);

/**
 * Free memory inside a response_t. Does not free `response` itself if it was allocated with malloc.
 */
void free_response(response_t *response);

/**
 * Returns the string message associated with the given HTTP code, eg. "Not Found" for 404.
 */
const char *http_code_to_str(httpcode_t code);

void print_request(const request_t *request);
void print_url(const url_t *url);

#endif