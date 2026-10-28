#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include "util.h"

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

/** Struct containing all information in an HTTP request */
typedef struct {
    int nlines;
    startline_t startline;
    linkedlist_t headers; // data will be a header_t*
    bool complete;
    bool keepalive;

    // private variables
    int _crlf_cntr; // counter for crlfs (used to detect when headers are done)
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
 * @param line Line string. Must be null terminated. May have trailing whitespace.
 * @param out Result request_t struct that will be filled in
 * @returns An HTTP status code. If OK is not returned, an error occurred and `out` will be not be
 *  valid.
 */
httpcode_t http_parse_request_line(char *line, request_t *out);

/**
 * Initializes a request_t. Must be called before `http_parse_request_line()`.
 */
void init_request(request_t *r);

/**
 * Free memory inside a request_t. Does not free `r` itself if it was allocated with malloc.
 */
void free_request(request_t *r);

/**
 * Parse the start line (the first line) of an HTTP request.
 * Format: `"[method] [uri] [version]"`
 * 
 * @param line Line string. Must be null terminated. May have trailing whitespace.
 * @param out Result startline_t struct that will be filled in
 * @returns An HTTP status code. If OK is not returned, an error occurred and `out` will be not be
 *  valid.
 */
httpcode_t http_parse_start_line(char *line, startline_t *out);

/**
 * Free memory inside a startline_t. Does not free `rl` itself if it was allocated with malloc.
 */
void free_startline(startline_t *s);

/**
 * Parse an HTTP header from a line string.
 * Format: `"[name]: [value]"`
 * 
 * @param line Line string. Must be null terminated. May have trailing whitespace.
 * @param out Result header_t struct that will be filled in
 * @returns An HTTP status code. If OK is not returned, an error occurred and `out` will be not be
 *  valid.
 */
httpcode_t http_parse_header(char *line, header_t *out);

/**
 * Free memory inside a header_t. Does not free `h` itself if it was allocated with malloc.
 */
void free_header(header_t *h);

/**
 * Make an HTTP response string given a status code, mimetype, and content. If mimetype or content
 * are NULL, they will not be included in the response.
 */
char* make_response(httpcode_t code, const char* mimetype, const unsigned char* content, int content_len);

/**
 * Returns the string message associated with the given HTTP code, eg. "Not Found" for 404.
 */
const char *code_to_msg(httpcode_t code);

#endif