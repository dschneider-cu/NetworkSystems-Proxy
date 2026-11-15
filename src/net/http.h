#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include "socket.h"
#include "../utils.h"

#define PORT_HTTP  80
#define PORT_HTTPS 443
#define PORT_MAX   65535

typedef enum {
    HTTP_OK                         = 200,
    HTTP_BAD_REQUEST                = 400,
    HTTP_FORBIDDEN                  = 403,
    HTTP_NOT_FOUND                  = 404,
    HTTP_METHOD_NOT_ALLOWED         = 405,
    HTTP_REQUEST_TIMEOUT            = 408,
    HTTP_CLIENT_CLOSED_REQUEST      = 499,
    HTTP_INTERNAL_SERVER_ERROR      = 500,
    HTTP_BAD_GATEWAY                = 502,
    HTTP_GATEWAY_TIMEOUT            = 504,
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
    vec_t headers; // data will be a header_t*
    bool complete; // Whether the request is done parsing
} request_t;

typedef struct {
    int nlines;
    httpcode_t code;
    char *version;
    char *status_text;
    vec_t headers; // data will be header_t*
    char *content;
    int content_len;
    bool complete;        // Whether the response is done parsing
} response_t;

typedef struct {
    const char *host; // Domain name
    int host_len;
    const char *path; // May be empty string
    int path_len;
    const char *query; // Stuff after question mark
    int query_len;
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
 * Adds a header to the request, or updates it if it already exists.
 * 
 * @param request Pointer to the request
 * @param name Header name
 * @param value New header value
 */
void http_set_request_header(request_t *request, const char *name, const char *value);

/**
 * Returns the value of the given header name.
 * 
 * @param response Pointer to the response
 * @param name Name of the header to fetch
 * @return String value of the header, or NULL if the response doesn't have the header
 */
const char *http_get_response_header(const response_t *response, const char *name);

/**
 * Adds a header to the response, or updates it if it already exists.
 * 
 * @param response Pointer to the response
 * @param name Header name
 * @param value New header value
 */
void http_set_response_header(response_t *response, const char *name, const char *value);

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
 * On timeout, outputs SOCKET_TIMEOUT and returns HTTP_REQUEST_TIMEOUT
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

/**
 * Callback for processing received response content. Chunk data exists only for the lifetime of
 * this function. Do not save the pointer.
 * 
 * @param response Pointer to the response
 * @param chunk Pointer to the chunk data
 * @param chunk_len Length of the chunk in bytes
 * @param ctx Arbitrary context to pass in
 * @return An HTTP status code representing the result of processing the chunk. Anything other than
 *      `HTTP_OK` will cancel the receive.
 */
typedef httpcode_t (*resp_content_callback_t)(
    response_t *response,
    char *chunk,
    int chunk_len,
    void *ctx
);

/**
 * Receives an HTTP response from a socket reader and outputs a parsed response.
 * 
 * On timeout, outputs SOCKET_TIMEOUT and returns HTTP_GATEWAY_TIMEOUT
 * 
 * @param reader Socket reader (must be initialized already)
 * @param out_sockcode Output parameter for the socket result code
 * @param out_response Output parameter for the parsed HTTP response
 * @return An HTTP status code representing the result of the receive
 */
httpcode_t http_receive_response(
    sockreader_t *reader,
    int *out_sockcode,
    response_t *out_response
);

/**
 * Receives HTTP response content from a socket reader. The content will be received in chunks and
 * processed by calling `content_callback`. Does nothing if the response has no content.
 * 
 * On timeout, outputs SOCKET_TIMEOUT and returns HTTP_GATEWAY_TIMEOUT
 * 
 * @param reader Socket reader (must be initialized already)
 * @param out_sockcode Output parameter for the socket result code
 * @param response Output parameter for the parsed HTTP response
 * @param content_callback Callback function to process chunks of the received content. See 
 *      `resp_content_callback_t`
 * @param ctx Arbitrary context which will be passed into `content_callback`
 * @return An HTTP status code representing the result of the receive
 */
httpcode_t http_receive_response_content(
    sockreader_t *reader,
    int *out_sockcode,
    response_t *response,
    resp_content_callback_t content_callback,
    void *ctx
);

/** Calculates the size in bytes required for an HTTP header. Includes \r\n. */
int http_header_text_size(const header_t *header);

/** Calculates the text length required for an HTTP request. (Does not include null terminator) */
int http_request_text_size(const request_t *request);

/**
 * Calculates the size in bytes required for an HTTP response. (Including content if it has it)
 * (Does not include null terminator)
 */
int http_response_text_size(const response_t *response);

/**
 * Converts an HTTP request to text and returns a heap-allocated buffer. Does nothing if the
 * request is incomplete.
 * 
 * @param request HTTP request
 * @param out_len Output param for the length of the text in bytes
 * @return A heap-allocated buffer for the text + null terminator.
 */
char *http_make_request_text(const request_t *request, int *out_len);

/**
 * Converts an HTTP response to text and returns a heap-allocated buffer. Does nothing if the
 * response is incomplete.
 * 
 * @param response HTTP response
 * @param out_len Output param for the length of the text in bytes
 * @return A heap-allocated buffer for the text + null terminator.
 */
char *http_make_response_text(const response_t *response, int *out_len);

/**
 * Sends a GET request given a URL string and receives a response. Response content will be
 * received in chunks and processed by `content_callback`.
 * 
 * @param url URL string to fetch
 * @param out_sockcode Output param for the socket code
 * @param content_callback Callback function to process chunks of the received response content.
 *      See `resp_content_callback_t`
 * @param ctx Arbitrary context which will be passed into `content_callback`
 * @param timeout_sec Seconds to wait before timing out
 * @return A heap-allocated response.
 */
response_t *http_send_and_receive(
    const char *url,
    int *out_sockcode,
    resp_content_callback_t content_callback,
    void *ctx,
    int timeout_sec
);

/**
 * Send an HTTP request on the given socket.
 * 
 * @param request HTTP request
 * @param sock Socket to send on. Must be a client socket.
 * @return `SOCKET_SUCCESS` on success, `SOCKET_CLOSED` on graceful shutdown, or `SOCKET_ERR` on
 *      error.
 */
int http_send_request(const request_t *request, const socket_t *sock);

/**
 * Send an HTTP response on the given socket.
 * 
 * @param request HTTP response
 * @param sock Socket to send on. Must be a client socket.
 * @return `SOCKET_SUCCESS` on success, `SOCKET_CLOSED` on graceful shutdown, or `SOCKET_ERR` on
 *      error.
 */
int http_send_response(const response_t *response, const socket_t *sock);

/**
 * Create a request_t from strings
 *
 * @param out_request Output param for the request
 * @param method HTTP method ("GET")
 * @param url Request url
 * @param version Version ("HTTP/1.0")
 * @param headers Array of headers; must be pairs of strings [name, value, name, value, ...]
 * @param nheaders Number of headers, should be length of `headers` / 2
 */
void http_make_request(
    request_t *out_request,
    const char *method,
    const char *url,
    const char *version,
    const char **headers,
    int nheaders
);

/**
 * Create a response_t
 * 
 * @param out_response Output param for the response
 * @param code HTTP response code
 * @param headers Array of headers; must be pairs of strings [name, value, name, value, ...]
 * @param nheaders Number of headers, should be length of `headers` / 2
 */
void http_make_response(
    response_t *out_response,
    httpcode_t code,
    const char **headers,
    int nheaders,
    const char *content,
    int content_len
);

/**
 * Make a simple response with just a status code.
 * 
 * @param out_response Output param for the response
 * @param code Response status code
 */
void http_make_simple_response(response_t *out_response, httpcode_t code);

/**
 * Free memory inside a header_t. Does not free `header` itself if it was allocated with malloc.
 * No-op if header is already freed.
 */
void free_header(header_t *header);

/**
 * Free memory inside a request_t. Does not free `request` itself if it was allocated with malloc.
 * No-op if request is already freed.
 */
void free_request(request_t *request);

/**
 * Free memory inside a response_t. Does not free `response` itself if it was allocated with malloc.
 * No-op if response is already freed.
 */
void free_response(response_t *response);

/**
 * Returns the string message associated with the given HTTP code, eg. "Not Found" for 404.
 */
const char *httpcode_to_str(httpcode_t code);

void print_request(const request_t *request);
void print_response(const response_t *response);
void print_url(const url_t *url);

#endif