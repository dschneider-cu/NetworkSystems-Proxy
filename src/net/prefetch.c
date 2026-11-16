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

// State machine for the HTML parser
typedef enum {
    SEARCHING,       // Searching for anchor tag
    IN_TAG,          // In tag ("div ")
    IN_ATTRIBUTES,   // In <a> attribute list
    IN_ATTRIBUTE,    // In a specific attribute of <a>
    IN_HREF,         // Inside quotes of href attribute (found a link!)
    IN_VALUE,        // Inside quotes of an attribute that isn't href
    IN_COMMENT,      // Inside "<!-- ..."
    IN_COMMENT_CLOSE // Inside "-->"
} html_state_t;

/**
 * State transitions (roughly)
 * 
 * SEARCHING --'<'--> IN_TAG
 * SEARCHING --anything else--> SEARCHING
 * 
 * IN_TAG --token="a "--> IN_ATTRIBUTES
 * IN_TAG --token="!--"--> IN_COMMENT
 * IN_TAG --'>'--> SEARCHING
 * 
 * IN_ATTRIBUTES --whitespace--> IN_ATTRIBUTES
 * IN_ATTRIBUTES --'>'--> SEARCHING
 * IN_ATTRIBUTES --anything else--> IN_ATTRIBUTE
 * 
 * IN_ATTRIBUTE --'"' && token="href=\""--> IN_HREF
 * IN_ATTRIBUTE --'"' else--> IN_VALUE
 * 
 * IN_VALUE --'"'--> IN_ATTRIBUTES
 * 
 * IN_HREF --'"'--> SEARCHING, export link
 * 
 * IN_COMMENT --'-'--> IN_COMMENT_CLOSE
 * 
 * IN_COMMENT_CLOSE --token="-->"--> SEARCHING
 * IN_COMMENT_CLOSE --token_len>3--> IN_COMMENT
 */

/**
 * Parses <a> tags in the given html and returns a vec of links.
 * 
 * @param html HTML string to parse
 * @return Vector of heap-allocated strings
 */
vec_t pf_extract_links(const char *html) {
    vec_t links;
    vec_init(&links);
    
    const char *p = html; // current position
    html_state_t state = SEARCHING;
    const char *token;
    int token_len = 0;

    while (*p) {
        char c = *p;
        p++; // consume character
        char next = *p;

        // - State machine monstrosity -
        switch (state) {
        case SEARCHING:
            if (c == '<') {
                state = IN_TAG;
                token = p;
                token_len = 0;
            }
            break;
        
        case IN_TAG:
            token_len++;
            if (c == '>') {
                state = SEARCHING;
            } else if (iswhitespace(next)) {
                if (strneq(token, "a", token_len)) {
                    // Found anchor tag
                    state = IN_ATTRIBUTES;
                } else if (strneq(token, "!--", token_len)) {
                    state = IN_COMMENT;
                } else {
                    state = SEARCHING;
                }
            }
            break;
        
        case IN_ATTRIBUTES:
            if (c == '>' || next == '>') {
                state = SEARCHING;
            } else if (!iswhitespace(next)) {
                state = IN_ATTRIBUTE;
                token = p;
                token_len = 0;
            }
            break;

        case IN_ATTRIBUTE:
            token_len++;
            if (c == '"') {
                if (strneq(token, "href=\"", token_len)) {
                    // found a link, hyah!
                    state = IN_HREF;
                    token = p;
                    token_len = 0;
                } else {
                    state = IN_VALUE;
                }
            }
            break;

        case IN_VALUE:
            if (c == '"') {
                state = IN_ATTRIBUTES;
            }
            break;

        case IN_HREF:
            if (c == '"') {
                // Push link
                char *link = strnclone(token, token_len);
                vec_push(&links, link);
                state = IN_ATTRIBUTES;
            } else {
                token_len++;
            }
            break;

        case IN_COMMENT:
            if (c == '-') {
                state = IN_COMMENT_CLOSE;
                token = p;
                token_len = 0;
            }
            break;

        case IN_COMMENT_CLOSE:
            if (token_len > 3) { // len("-->")
                state = IN_COMMENT;
            } else if (strneq(token, "-->", token_len)) {
                state = SEARCHING;
            } else {
                token_len++;
            }
            break;
        }
    }

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
void pf_fetch(const char *link, int ttl) {
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
        char *key = make_cache_key(url);
        int content_len;
        void *freeptr; 

        char *html = cache_get(&entry, key, &content_len, &freeptr);
        if (html == NULL) {
            free(key);
            cache_close_entry(&entry);
            continue;
        }

        // Parse HTML for links
        vec_t links = pf_extract_links(html);

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