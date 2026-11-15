#include "prefetch.h"
#include "http.h"
#include "socket.h"
#include "cache.h"
#include "../utils.h"

// Thread-safe queue using a linked list
typedef struct {
    linkedlist_t list;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} pfqueue_t;

// Global prefetch queue
static pfqueue_t pfq;


/** Enqueue a cache key into the queue */
void pfq_enqueue(char *cache_key) {
    pthread_mutex_lock(&pfq.lock);
    linkedlist_push(&pfq.list, cache_key);
    pthread_mutex_unlock(&pfq.lock);
    // Signal to waiter thread that data is available
    pthread_cond_signal(&pfq.cond);
}

/** Dequeue a cache key from the global pfq. Returns null if queue is empty. */
char *pfq_dequeue() {
    pthread_mutex_lock(&pfq.lock);
    while (true) {
        int size = linkedlist_size(&pfq.list);

        if (size == 0) {
            // Queue is empty, wait for condition variable to be notified. Lock released while
            // waiting.
            pthread_cond_wait(&pfq.cond, &pfq.lock);
        } else {
            // Queue is not empty. Lock guaranteed by c.wait() or lk.lock()
            char *cache_key = (char *)linkedlist_popleft(&pfq.list);
            pthread_mutex_unlock(&pfq.lock);
            return cache_key;
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

}

void pf_fetch(char *link) {

}

void *pf_thread_main(void *arg) {
    while (true) {
        char *cache_key = pfq_dequeue();
        if (cache_key == NULL) continue;

        // Get cache entry
        // Call pf_read_links
        // Fetch each link
    }
}

pthread_t prefetch_launch_thread() {
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, pf_thread_main, NULL);
    return thread_id;
}

void prefetch_enqueue(const url_t *url) {
    pfq_enqueue(make_cache_key(url));
}