#include <stdio.h>
#include <stdbool.h>
#include "net/prefetch.h"
#include "net/http.h"

bool VERBOSE = true;

httpcode_t content_callback(response_t *response, char *chunk, int chunk_len, void *ctx) {
    int *i = (int *)ctx;
    vlog("Chunk %d [%d bytes] %.*s\n", *i, chunk_len, chunk_len, chunk);
    (*i)++;
    return HTTP_OK;
}

int main() {
    printf("Hello world\n");

    // request_t req;
    // const char *headers[] = {"Host", "netsys.cs.colorado.edu"};
    // http_make_request(&req, "GET", "/index.html", "HTTP/1.1", headers, 1);
    // print_request(&req);
    // printf("\n\n");

    pf_fetch("http://netsys.cs.colorado.edu/index.html", 20);

    // free_request(&req);

    return 0;
}