#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "http.h"


httpcode_t http_parse_request_line(char *line, request_t *out) {
    if (line == NULL || out == NULL) { return HTTP_INTERNAL_SERVER_ERROR; }
    if (out->complete) { return HTTP_OK; }

    httpcode_t code = HTTP_INTERNAL_SERVER_ERROR;

    if (out->nlines == 0) {
        // First line
        code = http_parse_start_line(line, &out->startline);
    } else if (strcmp(line, "\r\n")) {
        // Blank line
        out->_crlf_cntr++;
        code = HTTP_OK;
    } else if (out->_crlf_cntr < 0) {
        // Header line
        header_t *h = malloc(sizeof(header_t));
        code = http_parse_header(line, h);
        if (code == HTTP_OK) {
            linkedlist_push(&out->headers, h);

            // Check keepalive header
            if (streq_icase(h->name, "Connection") && streq_icase(h->value, "keep-alive")) {
                out->keepalive = true;
            }
        }
    } else if (out->_crlf_cntr >= 2) {
        // Two blank lines mark the end of the headers. Request completed.
        out->complete = true;
        code = HTTP_OK;
    } else {
        return HTTP_BAD_REQUEST;
    }

    if (code != HTTP_OK) { return code; }
    out->nlines++;
    return HTTP_OK;
}

void init_request(request_t *r) {
    r->nlines = 0;
    r->complete = false;
    r->keepalive = false;
    r->startline.method = NULL;
    r->startline.uri = NULL;
    r->startline.version = NULL;
    r->_crlf_cntr = 0;
    linkedlist_init(&r->headers);
}

void free_request(request_t *r) {
    free_startline(&r->startline);
    linkedlist_destroy(&r->headers);
}

httpcode_t http_parse_start_line(char *line, startline_t *out) {
    if (line == NULL || out == NULL) { return HTTP_INTERNAL_SERVER_ERROR; } 
    int length = rstrip(line);

    // start of current segment (method, uri, or version) in line
    int i = 0;
    // end of current segment in line
    int j = 0;

    // Find end of Method
    while (j < length && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string
    
    // Copy characters from i to j into method
    out->method = malloc(j - i + 1);
    memcpy(out->method, &line[i], j - i);
    out->method[j - i] = '\0';
    // Consume space & move forward
    j++;
    i = j;

    // Find end of URI
    while (j < length && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string
    
    // Copy characters from i to j into uri
    out->uri = malloc(j - i + 1);
    memcpy(out->uri, &line[i], j - i);
    out->uri[j - i] = '\0';
    // Consume space & move forward
    j++;
    i = j;

    // Find end of Version
    while (j < length && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string

    // Copy characters from i to j into version
    out->version = malloc(j - i + 1);
    memcpy(out->version, &line[i], j - i);
    out->version[j - i] = '\0';
    
    return HTTP_OK;
}

void free_startline(startline_t *s) {
    free(s->method);
    free(s->uri);
    free(s->version);
}

httpcode_t http_parse_header(char *line, header_t *out) {
    if (line == NULL || out == NULL) { return HTTP_INTERNAL_SERVER_ERROR; } 
    int length = rstrip(line);
    

    // Find the colon
    int i = 0;
    while (i < length && line[i] != ':') { i++; }
    if (i == length) { return HTTP_BAD_REQUEST; } // no colon found

    // Trim leading whitespace on value
    int j = i + 1;
    while (j < length && line[j] == ' ') { j++; }

    out->name = malloc(i + 1);
    memcpy(out->name, line, i);
    out->name[i] = '\0';

    out->value = malloc(length - j + 1);
    memcpy(out->value, &line[j], length - j);
    out->value[length - j] = '\0';

    return HTTP_OK;
}

void free_header(header_t *h) {
    free(h->name);
    free(h->value);
}

char *make_response(httpcode_t code, const char* mimetype, const unsigned char* content, int content_len) {
    if (mimetype == NULL || content == NULL || content_len == 0) {
        char *resp = malloc(256); // enough space for response
        sprintf(resp, "HTTP/1.1 %d %s\r\n\r\n", code, code_to_msg(code));
        return resp;
    } else {
        char *resp = malloc(256 + content_len); // enough space for headers & content
        int bytes_written = sprintf(
            resp,
            "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
            code,
            code_to_msg(code),
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

const char *code_to_msg(httpcode_t code) {
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