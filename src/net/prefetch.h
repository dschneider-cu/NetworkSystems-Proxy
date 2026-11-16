#ifndef PREFETCH_H
#define PREFETCH_H

#include <pthread.h>
#include "http.h"

/** Launch thread to handle prefetching */
pthread_t prefetch_launch_thread();

/**
 * Enqueue a cache key to be prefetched. This will read the cache entry with the given key, parse
 * <a> tags, fetch the corresponding link, and place it in the cache.
 * 
 * @param url Parsed URL of the page to prefetch. The page at this url must already be in the
 *      cache.
 */
void prefetch_enqueue(const url_t *url);

#endif