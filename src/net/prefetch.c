#include <stdlib.h>
#include "prefetch.h"
#include "socket.h"
#include "cache.h"
#include "../utils.h"

#define PREFETCH_TIMEOUT_SEC 5

// Thread-safe queue using a linked list
typedef struct {
    linkedlist_t list; // url_t*
    pthread_mutex_t lock;
    pthread_cond_t cond;
} pfqueue_t;

// Global prefetch queue
static pfqueue_t pfq;


/** Enqueue a cache key into the queue */
void pfq_enqueue(url_t *url) {
    pthread_mutex_lock(&pfq.lock);
    linkedlist_push(&pfq.list, url);
    pthread_mutex_unlock(&pfq.lock);
    // Signal to waiter thread that data is available
    pthread_cond_signal(&pfq.cond);
}

/** Dequeue a cache key from the global pfq. Returns null if queue is empty. */
url_t *pfq_dequeue() {
    pthread_mutex_lock(&pfq.lock);
    while (true) {
        int size = linkedlist_size(&pfq.list);

        if (size == 0) {
            // Queue is empty, wait for condition variable to be notified. Lock released while
            // waiting.
            pthread_cond_wait(&pfq.cond, &pfq.lock);
        } else {
            // Queue is not empty. Lock guaranteed by c.wait() or lk.lock()
            url_t *url = (url_t *)linkedlist_popleft(&pfq.list);
            pthread_mutex_unlock(&pfq.lock);
            return url;
        }
    }
}

/**
 * Parses <a> tags in the given html and returns a vec of links.
 * 
 * @param html HTML string to parse (will be mutated)
 * @return Vector of strings. Each string will be a pointer into `html`
 */
vec_t pf_read_links(char *html) {
    vec_t links;
    vec_init(&links);
    
    // Parse html

    return links;
}

httpcode_t pf_content_callback(response_t *response, char *chunk, int chunk_len, void *ctx) {
    // Put in cache
}

void pf_fetch(char *link) {
    // Check if already in cache

    request_t request;
    http_make_request(&request, "GET", link, "HTTP/1.1", NULL, 0);

    int sockcode;
    response_t *response = http_send_and_receive(
        &request,
        &sockcode,
        pf_content_callback,
        NULL, // cache entry
        PREFETCH_TIMEOUT_SEC
    );

    free_request(&request);
    free_response(&response);
}

void *pf_thread_main(void *arg) {
    while (true) {
        url_t *url = pfq_dequeue();
        if (url == NULL) continue;

        // Get cache entry
        // Call pf_read_links
        // Fetch each link

        free_url_copy(url);
    }
}

pthread_t prefetch_launch_thread() {
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, pf_thread_main, NULL);
    return thread_id;
}

/** returns heap-allocated copy of a url_t */
url_t *copy_url(const url_t *url) {
    url_t *cpy = (url_t *)malloc(sizeof(url_t));
    cpy->host = strclone(url->host);
    cpy->host_len = url->host_len;
    cpy->path = strclone(url->path);
    cpy->path_len = url->path_len;
    cpy->query = strclone(url->query);
    cpy->query_len = url->query_len;
    cpy->port = url->port;
}

void free_url_copy(url_t *url) {
    free(url->host);
    free(url->path);
    free(url->query);
    free(url);
}

void prefetch_enqueue(const url_t *url) {
    pfq_enqueue(copy_url(url));
}