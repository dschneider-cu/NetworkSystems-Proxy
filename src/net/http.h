#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include "../utils.h"

/** Struct for an HTTP header */
typedef struct {
    char *name;
    char *value;
} header_t;

/** Struct for the first line of an HTTP request */
typedef struct {
    char *method;
    char *uri;
    char *version;
} startline_t;

/**
 * Struct containing all information in an HTTP request: startline (method, uri, version), and a
 * list of headers.
 * (request content, ie POST is not supported)
 */
typedef struct {
    int nlines;
    startline_t startline;
    linkedlist_t headers; // data will be a header_t*
    bool complete; // Whether the request is done parsing

    // Private variables
    // counter for crlfs (used to detect when headers are done)
    int _crlf_cntr;
} request_t;

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
 * @param line Line string. Must be null terminated. May have trailing whitespace.
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_request_line(request_t *request, char *line);

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
 * Format: `"[method] [uri] [version]"`
 * 
 * @param line Line string. Must be null terminated. May have trailing whitespace.
 * @param out Result startline_t struct that will be filled in
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_start_line(startline_t *startline, char *line);

/**
 * Free memory inside a startline_t. Does not free `s` itself if it was allocated with malloc.
 */
void free_startline(startline_t *startline);

/**
 * Parse an HTTP header from a line string.
 * Format: `"[name]: [value]"`
 * 
 * @param header header_t struct that will be filled in
 * @param line Line string. Must be null terminated. May have trailing whitespace.
 * @returns An HTTP status code representing the result of the parsing
 */
httpcode_t http_parse_header(header_t *header, char *line);

/**
 * Returns the value of the given header name.
 * 
 * @param request Pointer to the request
 * @param name Name of the header to fetch
 * @return String value of the header, or NULL if the request doesn't have the header
 */
const char *http_get_header(const request_t *request, const char *name);

/**
 * Free memory inside a header_t. Does not free `h` itself if it was allocated with malloc.
 */
void free_header(header_t *header);

/**
 * Make an HTTP response string given a status code, mimetype, and content. If mimetype or content
 * are NULL, they will not be included in the response.
 */
char* make_response(httpcode_t code, const char* mimetype, const unsigned char* content, int content_len);

/**
 * Returns the string message associated with the given HTTP code, eg. "Not Found" for 404.
 */
const char *code_to_str(httpcode_t code);

#endif