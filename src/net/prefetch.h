#ifndef PREFETCH_H
#define PREFETCH_H

#include <pthread.h>
#include "http.h"

/**
 * Launch a thread to handle prefetching.
 * 
 * After caching a response, call `prefetch_enqueue(url)` to fetch links in the HTML asynchronously
 * and put the pages in the cache.
 * 
 * @param cache_ttl Time to live for prefetched cache entries
 * @return ID of the spawned thread
 */
pthread_t prefetch_launch_thread(int cache_ttl);

/**
 * Enqueue a cache key to be prefetched. This will read the cache entry with the given key, parse
 * <a> tags, fetch the corresponding link, and place it in the cache.
 * 
 * @param url Parsed URL of the page to prefetch. The page at this url must already be in the
 *      cache.
 */
void prefetch_enqueue(const url_t *url);


// todo remove
void pf_fetch(const char *link, int ttl);
vec_t pf_extract_links(const char *html);

#endif