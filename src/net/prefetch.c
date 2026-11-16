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
    return cpy;
}

void free_url_copy(url_t *url) {
    free((char *)url->host);
    free((char *)url->path);
    free((char *)url->query);
    free(url);
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

typedef struct {
    int *chunk_ctr;
    char *cache_key;
    cache_entry_t *cache_entry;
    int ttl;
} pf_fetch_callback_ctx_t;

httpcode_t pf_content_callback(response_t *response, char *chunk, int chunk_len, void *ctx) {
    pf_fetch_callback_ctx_t *pfctx = (pf_fetch_callback_ctx_t *)ctx;
    int chunk_i = *pfctx->chunk_ctr;
    cache_entry_t *entry = pfctx->cache_entry;
    char *key = pfctx->cache_key;

    if (chunk_i == 0) {
        // First chunk - put in cache
        cache_put(entry, key, chunk, chunk_len, pfctx->ttl);
    } else {
        // Subsequent chunks - append
        cache_append(entry, chunk, chunk_len);
    }

    // increment chunk counter
    (*pfctx->chunk_ctr)++;
    return HTTP_OK;
}

/** Fetches the given link and puts it in the cache */
void pf_fetch(char *link, int ttl) {
    request_t request;
    http_make_request(&request, "GET", link, "HTTP/1.1", NULL, 0);

    // Parse link
    url_t url;
    httpcode_t code = http_parse_url(&url, &request);
    if (code != HTTP_OK) {
        elog("Failed to parse link for prefetching: %s", link);
        free_request(&request);
        return;
    }

    // Check if already in cache
    char *cache_key = make_cache_key(&url);
    if (cache_has(cache_key)) {
        // cache entry already exists. Skip
        free_request(&request);
        free(cache_key);
        return;
    }

    // Fix request path & Host header
    http_move_host_to_header(&request, &url);

    // Context for content callback
    cache_entry_t cache_entry;
    init_cache_entry(&cache_entry);
    int chunk_ctr = 0;
    pf_fetch_callback_ctx_t ctx = {
        &chunk_ctr,
        cache_key,
        &cache_entry,
        ttl
    };
    
    int sockcode;
    response_t *response = http_send_and_receive(
        &request,
        &sockcode,
        pf_content_callback,
        &ctx,
        PREFETCH_TIMEOUT_SEC
    );

    cache_close_entry(&cache_entry);
    free(cache_key);
    free_request(&request);
    free_response(response);
    free(response);
}

void *pf_thread_main(void *arg) {
    int ttl = *((int *)arg);
    free(arg);

    while (true) {
        url_t *url = pfq_dequeue();
        if (url == NULL) continue;

        // Get HTML from cache entry
        cache_entry_t entry;
        init_cache_entry(&entry);
        char *key = make_cache_key(&url);
        int content_len;
        void *freeptr; 

        char *html = cache_get(&entry, key, &content_len, &freeptr);
        if (html == NULL) {
            free(key);
            cache_close_entry(&entry);
            continue;
        }

        // Parse HTML for links
        vec_t links = pf_read_links(html);

        // Fetch links & place in cache
        for (int i = 0; i < vec_size(&links); i++) {
            const char *link = vec_get(&links, i);
            pf_fetch(link, ttl);
        }

        // Clean up
        vec_free_each(&links);
        vec_free(&links);
        free_url_copy(url);
        free(key);
        free(freeptr);
    }
}

pthread_t prefetch_launch_thread(int cache_ttl) {
    int *ttl_ptr = (int *)malloc(sizeof(int));
    *ttl_ptr = cache_ttl;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, pf_thread_main, ttl_ptr);
    return thread_id;
}

void prefetch_enqueue(const url_t *url) {
    pfq_enqueue(copy_url(url));
}