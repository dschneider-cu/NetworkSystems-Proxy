#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "net/prefetch.h"
#include "net/cache.h"
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

    const char *link = "http://netsys.cs.colorado.edu/index.html";
    url_t url;
    request_t rq;
    http_make_request(&rq, "GET", link, "HTTP/1.1", NULL, 0);
    http_parse_url(&url, &rq);
    http_move_host_to_header(&rq, &url);

    print_request(&rq);
    printf("\n\n");

    cache_entry_t entry;
    init_cache_entry(&entry);
    char *key = make_cache_key(&url);
    int content_len;
    void *freeptr; 

    char *html = cache_get(&entry, key, &content_len, &freeptr);
    // printf("%s\n", html);
    
    vec_t links = pf_extract_links(html);
    print_vec(&links);
    
    free_request(&rq);
    free(key);
    free(freeptr);
    return 0;
}