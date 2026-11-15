#include <stdio.h>
#include <stdbool.h>
#include "net/prefetch.h"
#include "net/http.h"

bool VERBOSE = true;

int main() {
    printf("Hello world\n");

    request_t r;
    http_make_request(&r, "GET", "http://example.foo/hello?val=abcdefg", "HTTP/1.1", NULL, 0);

    url_t url;
    http_parse_url(&url, &r);

    printf("Host: \"%.*s\"\n", url.host_len, url.host);
    printf("Path: \"%.*s\"\n", url.path_len, url.path);
    printf("Query: \"%.*s\"\n", url.query_len, url.query);
    printf("Port: %d\n", url.port);

    free_request(&r);

    return 0;
}