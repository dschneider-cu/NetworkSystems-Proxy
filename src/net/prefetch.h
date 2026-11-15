#ifndef PREFETCH_H
#define PREFETCH_H

#include <pthread.h>

/** Launch thread to handle prefetching */
pthread_t prefetch_launch_thread();

/**
 * Enqueue a cache key to be prefetched. This will read the cache entry with the given key, parse
 * <a> tags, fetch the corresponding link, and place it in the cache.
 * 
 * @param cache_key URL used as a cache key for the cache entry.
 */
void prefetch_enqueue(const char *cache_key);

#endif