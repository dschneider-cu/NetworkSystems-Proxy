#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "http.h"
#include "../utils.h"


httpcode_t http_parse_request_line(request_t *request, char *line, int line_len) {
    if (request == NULL || line == NULL) { return HTTP_INTERNAL_SERVER_ERROR; }
    if (request->complete) { return HTTP_OK; }

    httpcode_t code = HTTP_INTERNAL_SERVER_ERROR;
    line_len = rnstrip(line, line_len);
    printf("%.*s [%d]\n", line_len, line, line_len);

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

void init_request(request_t *request) {
    request->nlines = 0;
    request->complete = false;
    request->method = NULL;
    request->url = NULL;
    request->version = NULL;
    linkedlist_init(&request->headers);
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

bool _header_match(int i, const void *data, const void *criteria) {
    const char *name = (char *)criteria;
    header_t *header = (header_t *)data;
    return streq(header->name, name);
}

const char *http_get_request_header(const request_t *request, const char *name) {
    header_t *header = (header_t *)linkedlist_find(&request->headers, _header_match, name);
    if (header != NULL) {
        return header->value;
    } else {
        return NULL;
    }
}

const char *http_get_response_header(const response_t *response, const char *name) {
    header_t *header = (header_t *)linkedlist_find(&response->headers, _header_match, name);
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
    int sockcode;
    httpcode_t httpcode;
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
    response_t *out_response,
    sockread_callback_t resp_content_callback
) {
    return HTTP_INTERNAL_SERVER_ERROR;
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

char *http_make_response(httpcode_t code, const char* mimetype, const unsigned char* content, int content_len) {
    if (mimetype == NULL || content == NULL || content_len == 0) {
        char *resp = (char *)malloc(256); // enough space for response
        sprintf(resp, "HTTP/1.1 %d %s\r\n\r\n", code, http_code_to_str(code));
        return resp;
    } else {
        char *resp = (char *)malloc(256 + content_len); // enough space for headers & content
        int bytes_written = sprintf(
            resp,
            "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
            code,
            http_code_to_str(code),
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

void free_header(header_t *header) {
    free(header->name);
    free(header->value);
}

bool _ll_freeheader(int i, void *data) {
    free_header((header_t *)data);
    return false;
}
void free_request(request_t *request) {
    if (request->method != NULL) free(request->method);
    if (request->url != NULL) free(request->url);
    if (request->version != NULL) free(request->version);
    linkedlist_foreach(&request->headers, _ll_freeheader);
    linkedlist_free(&request->headers, true);
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
        case HTTP_HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version Not Supported";
        default:
            return "Internal Server Error";
    }
}

bool _print_header(int i, void *data) {
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
        request->version, request->method, request->url
    );

    linkedlist_foreach(&request->headers, _print_header);
}

void print_url(const url_t *url) {
    printf(
        "- URL -\nHost: \"%.*s\"\nPath: \"%.*s\"\nPort: %d\n",
        url->host_len, url->host, url->path_len, url->path, url->port
    );
}