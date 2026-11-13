#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "http.h"
#include "../utils.h"


httpcode_t http_parse_request_line(request_t *request, char *line, int line_len) {
    if (request == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; }
    if (request->complete) { return HTTP_OK; }

    httpcode_t code = HTTP_INTERNAL_SERVER_ERROR;

    // Strip whitespace
    line = (char *)nstrip(line, line_len, &line_len);

    if (request->nlines == 0) {
        // First line
        code = http_parse_start_line(request, line, line_len);
    } else if (line_len == 0) {
        // Blank line; end of headers
        request->complete = true;
        code = HTTP_OK;
    } else if (line_len > 0) {
        // Header line
        header_t *h = (header_t *)malloc(sizeof(header_t));
        code = http_parse_header(h, line, line_len);
        if (code == HTTP_OK) {
            linkedlist_push(&request->headers, h);
        }
    } else {
        return HTTP_BAD_REQUEST;
    }

    if (code != HTTP_OK) { return code; }
    request->nlines++;
    return HTTP_OK;
}

httpcode_t http_parse_response_line(response_t *response, char *line, int line_len) {
    if (response == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; }
    if (response->complete) { return HTTP_OK; }

    httpcode_t code = HTTP_INTERNAL_SERVER_ERROR;

    // Strip whitespace
    line = (char *)nstrip(line, line_len, &line_len);

    if (response->nlines == 0) {
        // First line
        code = http_parse_status_line(response, line, line_len);
    } else if (line_len == 0) {
        // Blank line; end of headers
        response->complete = true;
        code = HTTP_OK;
    } else if (line_len > 0) {
        // Header line
        header_t *h = (header_t *)malloc(sizeof(header_t));
        code = http_parse_header(h, line, line_len);
        if (code == HTTP_OK) {
            linkedlist_push(&response->headers, h);
        }
    } else {
        return HTTP_BAD_REQUEST;
    }

    if (code != HTTP_OK) { return code; }
    response->nlines++;
    return HTTP_OK;
}

void init_request(request_t *request) {
    request->nlines = 0;
    request->complete = false;
    request->method = NULL;
    request->url = NULL;
    request->version = NULL;
    linkedlist_init(&request->headers);
}

void init_response(response_t *response) {
    response->nlines = 0;
    response->complete = false;
    response->code = HTTP_OK;
    response->content = NULL;
    response->content_len = 0;
    linkedlist_init(&response->headers);
}

httpcode_t http_parse_start_line(request_t *request, char *line, int line_len) {
    if (request == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; } 

    // start of current segment (method, url, or version) in line
    int i = 0;
    // end of current segment in line
    int j = 0;

    // Find end of Method
    while (j < line_len && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string
    
    // Copy characters from i to j into method
    request->method = strnclone(&line[i], j - i);
    // Consume space & move forward
    j++;
    i = j;

    // Find end of URL
    while (j < line_len && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string
    
    // Copy characters from i to j into url
    request->url = strnclone(&line[i], j - i);
    // Consume space & move forward
    j++;
    i = j;

    // Find end of Version
    while (j < line_len && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_REQUEST; } // Empty string

    // Copy characters from i to j into version
    request->version = strnclone(&line[i], j - i);
    
    return HTTP_OK;
}

httpcode_t http_parse_status_line(response_t *response, char *line, int line_len) {
    if (response == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; }

    // start of current segment in line
    int i = 0;
    // end of current segment in line
    int j = 0;

    // Find space (after version, before code)
    while (j < line_len && line[j] != ' ') { j++; }
    if (i == j) { return HTTP_BAD_GATEWAY; } // Malformed response

    // Copy characters from i to j into version
    response->version = strnclone(&line[i], j - i);
    // Consume space & move forward
    j++;
    i = j;

    // Find digits of code
    while (j < line_len && line[j] >= '0' && line[j] <= '9') { j++; }

    char c = line[j];
    line[j] = '\0';
    int code;
    int len;
    if (!strtoint(&line[i], &code, &len, false)) {
        line[j] = c;
        return HTTP_BAD_GATEWAY;
    }
    line[j] = c;

    response->code = (httpcode_t) code;
    return HTTP_OK;
}

httpcode_t http_parse_header(header_t *header, char *line, int line_len) {
    if (header == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; } 
    
    // Find the colon
    int i = 0;
    while (i < line_len && line[i] != ':') { i++; }
    if (i == line_len) { return HTTP_BAD_REQUEST; } // no colon found

    // Trim leading whitespace on value
    int j = i + 1;
    while (j < line_len && line[j] == ' ') { j++; }

    header->name = strnclone(line, i);
    header->value = strnclone(&line[j], line_len - j);

    return HTTP_OK;
}

bool _header_match(int i, const void *data, const void *criteria, void *ctx) {
    const char *name = (char *)criteria;
    header_t *header = (header_t *)data;
    return streq_icase(header->name, name);
}

const char *http_get_request_header(const request_t *request, const char *name) {
    header_t *header = (header_t *)linkedlist_find(&request->headers, _header_match, name, NULL);
    if (header != NULL) {
        return header->value;
    } else {
        return NULL;
    }
}

const char *http_get_response_header(const response_t *response, const char *name) {
    header_t *header = (header_t *)linkedlist_find(&response->headers, _header_match, name, NULL);
    if (header != NULL) {
        return header->value;
    } else {
        return NULL;
    }
}

httpcode_t http_parse_url(url_t *url, const request_t *request) {
    char *urlstr = request->url;

    // GET http://example.foo/hello HTTP/1.1
    //     ^url   ^host      ^path
    const char *host;
    int host_len = 0;
    bool host_in_url;
    const char *path;
    int path_len = 0;
    const char *portstr;
    int port_len = 0;
    int port;

    // Look for host in URL
    if (strstartswith(urlstr, "http://")) {
        host = urlstr + strlen("http://");
        port = PORT_HTTP;
        host_in_url = true;
    } else if (strstartswith(urlstr, "https://")) {
        host = urlstr + strlen("https://");
        port = PORT_HTTPS;
        host_in_url = true;
    } else {
        // Look for host in Host header
        host = http_get_request_header(request, "Host");
        port = PORT_HTTP;
        host_in_url = false;

        if (host == NULL) {
            // Request doesn't contain a host
            return HTTP_BAD_REQUEST;
        }
    }

    // Find length of the host by searching for a slash or colon
    while (host[host_len] != '\0' && host[host_len] != '/' && host[host_len] != ':') {
        host_len++;
    }
    
    portstr = host + host_len;
    if (portstr[0] == ':') {
        portstr++; // Skip colon
        // Host has a port
        if (!strtoint(portstr, &port, &port_len, true) || port < 0 || port > PORT_MAX) {
            // Stuff after the colon is not a valid port
            return HTTP_BAD_REQUEST;
        }
    }

    // Remainder of the URL is the path
    if (host_in_url) {
        path = portstr + port_len;
    } else {
        path = request->url;
    }
    path_len = strlen(path);
    
    url->host_len = host_len;
    url->host = host;
    url->path = path;
    url->path_len = path_len;
    url->port = port;
    return HTTP_OK;
}

httpcode_t http_receive_request(sockreader_t *reader, int *out_sockcode, request_t *out_request) {
    int sockcode = SOCKET_ERR;
    httpcode_t httpcode = HTTP_INTERNAL_SERVER_ERROR;
    init_request(out_request);

    while (!out_request->complete) {
        // Receive line
        sockcode = sockreader_readline(reader);
        if (sockcode != SOCKET_SUCCESS) {
            *out_sockcode = sockcode;
            free_request(out_request);
            return HTTP_INTERNAL_SERVER_ERROR;
        }

        // Temporarily add null terminator
        char c = reader->line[reader->len];
        reader->line[reader->len] = '\0';

        // Parse line
        httpcode = http_parse_request_line(out_request, reader->line, reader->len);
        if (httpcode != HTTP_OK) {
            *out_sockcode = SOCKET_SUCCESS;
            free_request(out_request);
            return httpcode;
        }

        // Restore character
        reader->line[reader->len] = c;
    }

    *out_sockcode = SOCKET_SUCCESS;
    return HTTP_OK;
}

httpcode_t http_receive_response(
    sockreader_t *reader,
    int *out_sockcode,
    response_t *out_response
) {
    int sockcode = SOCKET_ERR;
    httpcode_t httpcode = HTTP_INTERNAL_SERVER_ERROR;
    init_response(out_response);

    while (!out_response->complete) {
        // Receive line
        sockcode = sockreader_readline(reader);
        if (sockcode != SOCKET_SUCCESS) {
            *out_sockcode = sockcode;
            free_response(out_response);
            return HTTP_INTERNAL_SERVER_ERROR;
        }

        // Temporarily add null terminator
        char c = reader->line[reader->len];
        reader->line[reader->len] = '\0';

        // Parse line
        httpcode = http_parse_response_line(out_response, reader->line, reader->len);
        if (httpcode != HTTP_OK) {
            *out_sockcode = SOCKET_SUCCESS;
            free_response(out_response);
            return httpcode;
        }

        // Restore character
        reader->line[reader->len] = c;
    }

    // Get content length
    int content_len = 0;
    const char *cl_header = http_get_response_header(out_response, "Content-Length");
    if (cl_header != NULL && !strtoint(cl_header, &content_len, NULL, false)) {
        // Malformed Content-Length header value
        *out_sockcode = SOCKET_SUCCESS;
        free_response(out_response);
        return HTTP_BAD_REQUEST;
    }

    out_response->content_len = content_len;
    *out_sockcode = SOCKET_SUCCESS;
    return HTTP_OK;
}

typedef struct {
    response_t *resp;
    resp_content_callback_t callback;
    void *ctx;
} _resp_sockread_callback_ctx;

int _resp_sockread_callback(sockreader_t *reader, char *chunk, int chunk_len, void *ctx) {
    _resp_sockread_callback_ctx *respctx = (_resp_sockread_callback_ctx *)ctx;
    httpcode_t code = respctx->callback(respctx->resp, chunk, chunk_len, respctx->ctx);
    if (code != HTTP_OK) {
        return code;
    } else {
        return SOCKET_SUCCESS;
    }
}

httpcode_t http_receive_response_content(
    sockreader_t *reader,
    int *out_sockcode,
    response_t *response,
    resp_content_callback_t content_callback,
    void *ctx
) {
    if (response->content_len <= 0) {
        *out_sockcode = SOCKET_SUCCESS;
        return HTTP_OK;
    }

    _resp_sockread_callback_ctx respctx = {
        response,
        content_callback,
        ctx
    };
    
    int sockcode = sockreader_readbytes(
        reader, 
        response->content_len, 
        _resp_sockread_callback, 
        &respctx
    );

    if (sockcode <= 0) {
        // Socket error occurred
        *out_sockcode = sockcode;
        return HTTP_INTERNAL_SERVER_ERROR;
    } else if (sockcode != SOCKET_SUCCESS) {
        // HTTP error occurred
        *out_sockcode = SOCKET_SUCCESS;
        return (httpcode_t) sockcode;
    }
    
    *out_sockcode = SOCKET_SUCCESS;
    return HTTP_OK;
}

int http_header_text_size(const header_t *header) {
    // Add name length and value length
    // +2 for colon and space between name and value
    // +2 for \r\n
    return strlen(header->name) + strlen(header->value) + 4;
}

// Callback for summing header size linked list
bool _sum_header_size(int i, void *data, void *ctx) {
    int *sum = (int *)ctx;
    header_t *h = (header_t *)data;
    *sum += http_header_text_size(h);
    return false;
}

int http_request_text_size(const request_t *request) {
    // Add method, url, and version length
    int size = strlen(request->method) 
        + strlen(request->url) 
        + strlen(request->version);
    // +2 for spaces in between
    // +2 for \r\n
    size += 4;

    // Add headers to sum
    linkedlist_foreach(&request->headers, _sum_header_size, &size);

    // +2 for ending \r\n
    size += 2;

    return size;
}

int http_response_text_size(const response_t *response) {
    // Add version length, code length (converted to str), and code text length
    int size = strlen(response->version) 
        + intstrlen(response->code) 
        + strlen(http_code_to_str(response->code));
    // +2 for spaces in between
    // +2 for \r\n
    size += 4;

    // Add headers to sum
    linkedlist_foreach(&response->headers, _sum_header_size, &size);

    // Add content length
    if (response->content != NULL && response->content_len > 0) {
        // +2 for \r\n between headers and content
        size += 2;
        size += response->content_len;
    }

    // +2 for ending \r\n
    size += 2;

    return size;
}

// Callback for writing header text to a buffer
bool _write_header(int i, void *data, void *ctx) {
    header_t *h = (header_t *)data;
    char **buffer = (char **)ctx;
    *buffer += sprintf(*buffer, "%s: %s\r\n", h->name, h->value);
    return false;
}

void http_request_to_text(const request_t *request, char *buffer) {
    if (!request->complete) return;

    // Start line
    buffer += sprintf(
        buffer, "%s %s %s\r\n", 
        request->method, request->url, request->version
    );

    // Headers
    linkedlist_foreach(&request->headers, _write_header, &buffer);

    // Ending blank line & null terminator
    *buffer++ = '\r';
    *buffer++ = '\n';
    *buffer++ = '\0';
}

void http_response_to_text(const response_t *response, char *buffer) {
    if (!response->complete) return;

    // Status line
    buffer += sprintf(
        buffer, "%s %d %s\r\n", 
        response->version, response->code, http_code_to_str(response->code)
    );

    // Headers
    linkedlist_foreach(&response->headers, _write_header, &buffer);

    // Content
    if (response->content != NULL && response->content_len > 0) {
        *buffer++ = '\r';
        *buffer++ = '\n';
        memcpy(buffer, response->content, response->content_len);
    }

    // Ending blank line & null terminator
    *buffer++ = '\r';
    *buffer++ = '\n';
    *buffer++ = '\0';
}

int http_send_request(const request_t *request, const socket_t sock) {
    return SOCKET_ERR;
}

int http_send_response(const response_t *response, const socket_t sock) {
    return SOCKET_ERR;
}

request_t *http_make_request(
    const char *method,
    const char *url,
    const char *version,
    const char **headers,
    int nheaders
) {
    request_t *request = (request_t *)malloc(sizeof(request_t));
    if (request == NULL) return NULL;

    request->method = strclone(method);
    request->url = strclone(url);
    request->version = strclone(version);

    // Headers
    linkedlist_init(&request->headers);

    for (int i = 0; i < nheaders; i++) {
        header_t *h = (header_t *)malloc(sizeof(header_t));
        h->name = strclone(headers[i*2]);
        h->value = strclone(headers[i*2+1]);

        linkedlist_push(&request->headers, h);
    }

    request->nlines = nheaders + 1;
    request->complete = true;

    return request;
}

response_t *http_make_response(
    httpcode_t code,
    const char **headers,
    int nheaders,
    const char *content,
    int content_len
) {
    response_t *response = (response_t *)malloc(sizeof(response_t));
    if (response == NULL) return NULL;

    response->code = code;
    response->content_len = content_len;

    // Headers
    linkedlist_init(&response->headers);

    for (int i = 0; i < nheaders; i++) {
        header_t *h = (header_t *)malloc(sizeof(header_t));
        h->name = strclone(headers[i*2]);
        h->value = strclone(headers[i*2+1]);

        linkedlist_push(&response->headers, h);
    }

    // Copy content
    if (content_len > 0) {
        response->content = strnclone(content, content_len);
    } else {
        response->content = NULL;
    }

    response->nlines = nheaders + 1;
    response->complete = true;

    return response;
}

void free_header(header_t *header) {
    free(header->name);
    free(header->value);
}

bool _ll_freeheader(int i, void *data, void *ctx) {
    free_header((header_t *)data);
    return false;
}
void free_request(request_t *request) {
    if (request->method != NULL) free(request->method);
    if (request->url != NULL) free(request->url);
    if (request->version != NULL) free(request->version);
    linkedlist_foreach(&request->headers, _ll_freeheader, NULL);
    linkedlist_free(&request->headers, true);
}

void free_response(response_t *response) {
    if (response->content != NULL) free(response->content);
    linkedlist_foreach(&response->headers, _ll_freeheader, NULL);
    linkedlist_free(&response->headers, true);
}

const char *http_code_to_str(httpcode_t code) {
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
        case HTTP_INTERNAL_SERVER_ERROR:
            return "Internal Server Error";
        case HTTP_HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version Not Supported";
        default:
            return "Unknown HTTP code";
    }
}

bool _print_header(int i, void *data, void *ctx) {
    header_t *h = (header_t *)data;
    printf("%s: %s\n", h->name, h->value);
    return false;
}

void print_request(const request_t *request) {
    if (request == NULL) {
        printf("- HTTP Request -\nNULL\n");
        return;
    }

    printf(
        "- HTTP Request -\n[complete: %s nlines: %d]\n",
        request->complete ? "true" : "false", request->nlines
    );
    printf(
        "%s %s %s\n",
        request->method, request->url, request->version
    );

    linkedlist_foreach(&request->headers, _print_header, NULL);
}

void print_response(const response_t *response) {
    if (response == NULL) {
        printf("- HTTP Response -\nNULL\n");
        return;
    }

    printf(
        "- HTTP Response -\n[complete: %s nlines: %d]\n",
        response->complete ? "true" : "false", response->nlines
    );
    printf(
        "%s %d %s\n",
        response->version, response->code, http_code_to_str(response->code)
    );

    linkedlist_foreach(&response->headers, _print_header, NULL);
    
    if (response->content != NULL) {
        print_hexdump(response->content, response->content_len);
    }
}

void print_url(const url_t *url) {
    printf(
        "- URL -\nHost: \"%.*s\"\nPath: \"%.*s\"\nPort: %d\n",
        url->host_len, url->host, url->path_len, url->path, url->port
    );
}