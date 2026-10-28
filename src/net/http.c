#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "http.h"


httpcode_t http_parse_request_line(request_t *request, char *line) {
    if (request == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; }
    if (request->complete) { return HTTP_OK; }

    httpcode_t code = HTTP_INTERNAL_SERVER_ERROR;

    if (request->nlines == 0) {
        // First line
        code = http_parse_start_line(&request->startline, line);
    } else if (streq(line, "\r\n")) {
        // Blank line
        request->_crlf_cntr++;
        code = HTTP_OK;
    } else if (request->_crlf_cntr < 0) {
        // Header line
        header_t *h = (header_t *)malloc(sizeof(header_t));
        code = http_parse_header(h, line);
        if (code == HTTP_OK) {
            linkedlist_push(&request->headers, h);
        }
    } else if (request->_crlf_cntr >= 2) {
        // Two blank lines mark the end of the headers. Request completed.
        request->complete = true;
        code = HTTP_OK;
    } else {
        return HTTP_BAD_REQUEST;
    }

    if (code != HTTP_OK) { return code; }
    request->nlines++;
    return HTTP_OK;
}

void init_request(request_t *request) {
    request->nlines = 0;
    request->complete = false;
    request->startline.method = NULL;
    request->startline.uri = NULL;
    request->startline.version = NULL;
    request->_crlf_cntr = 0;
    linkedlist_init(&request->headers);
}

void free_request(request_t *request) {
    free_startline(&request->startline);
    linkedlist_free(&request->headers, true);
}

httpcode_t http_parse_start_line(startline_t *startline, char *line) {
    if (startline == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; } 
    int length = rstrip(line);

    // start of current segment (method, uri, or version) in line
    int i = 0;
    // end of current segment in line
    int j = 0;

    // Find end of Method
    while (j < length && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string
    
    // Copy characters from i to j into method
    startline->method = (char *)malloc(j - i + 1);
    memcpy(startline->method, &line[i], j - i);
    startline->method[j - i] = '\0';
    // Consume space & move forward
    j++;
    i = j;

    // Find end of URI
    while (j < length && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string
    
    // Copy characters from i to j into uri
    startline->uri = (char *)malloc(j - i + 1);
    memcpy(startline->uri, &line[i], j - i);
    startline->uri[j - i] = '\0';
    // Consume space & move forward
    j++;
    i = j;

    // Find end of Version
    while (j < length && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string

    // Copy characters from i to j into version
    startline->version = (char *)malloc(j - i + 1);
    memcpy(startline->version, &line[i], j - i);
    startline->version[j - i] = '\0';
    
    return HTTP_OK;
}

void free_startline(startline_t *startline) {
    free(startline->method);
    free(startline->uri);
    free(startline->version);
}

httpcode_t http_parse_header(header_t *header, char *line) {
    if (header == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; } 
    int length = rstrip(line);
    
    // Find the colon
    int i = 0;
    while (i < length && line[i] != ':') { i++; }
    if (i == length) { return HTTP_BAD_REQUEST; } // no colon found

    // Trim leading whitespace on value
    int j = i + 1;
    while (j < length && line[j] == ' ') { j++; }

    header->name = (char *)malloc(i + 1);
    memcpy(header->name, line, i);
    header->name[i] = '\0';

    header->value = (char *)malloc(length - j + 1);
    memcpy(header->value, &line[j], length - j);
    header->value[length - j] = '\0';

    return HTTP_OK;
}

bool _header_match(int i, const void *data, const void *criteria) {
    const char *name = (char *)criteria;
    header_t *header = (header_t *)data;
    return streq(header->name, name);
}

const char *http_get_header(const request_t *request, const char *name) {
    header_t *header = (header_t *)linkedlist_find(&request->headers, _header_match, name);
    if (header != NULL) {
        return header->value;
    } else {
        return NULL;
    }
}

void free_header(header_t *header) {
    free(header->name);
    free(header->value);
}

char *make_response(httpcode_t code, const char* mimetype, const unsigned char* content, int content_len) {
    if (mimetype == NULL || content == NULL || content_len == 0) {
        char *resp = (char *)malloc(256); // enough space for response
        sprintf(resp, "HTTP/1.1 %d %s\r\n\r\n", code, code_to_str(code));
        return resp;
    } else {
        char *resp = (char *)malloc(256 + content_len); // enough space for headers & content
        int bytes_written = sprintf(
            resp,
            "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
            code,
            code_to_str(code),
            mimetype,
            content_len
        );

        // Write content
        int i = 0;
        for (; i < content_len; i++) {
            resp[bytes_written + i] = (char)content[i];
        }
        // Write null terminator
        resp[bytes_written + i + 1] = '\0';
        return resp;
    }
}

const char *code_to_str(httpcode_t code) {
    switch (code) {
        case HTTP_OK:
            return "OK";
        case HTTP_BAD_REQUEST:
            return "Bad Request";
        case HTTP_FORBIDDEN:
            return "Forbidden";
        case HTTP_NOT_FOUND:
            return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case HTTP_HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version Not Supported";
        default:
            return "Internal Server Error";
    }
}