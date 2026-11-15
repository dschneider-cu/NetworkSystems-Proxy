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

    request_t req;
    const char *headers[] = {"Host", "netsys.cs.colorado.edu"};
    http_make_request(&req, "GET", "/index.html", "HTTP/1.1", headers, 1);
    print_request(&req);
    printf("\n\n");

    int sockcode;
    int i;
    response_t *resp = http_send_and_receive(
        &req,
        &sockcode,
        content_callback,
        &i,
        5
    );

    print_response(resp);

    free_request(&req);
    free_response(resp);

    return 0;
}