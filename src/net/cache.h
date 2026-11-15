#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include "../file.h"

typedef struct {
    char *filepath;
    FILE *file;
    const char *file_mode;
    // todo: expiry
} cache_entry_t;

/**
 * Puts an item into the cache.
 * 
 * @param out_entry Cache entry struct which will be filled in
 * @param key Identifier for the cache entry
 * @param content Pointer to the content to write into the cache
 * @param content_len Number of bytes in the content
 * @param ttl Time to live, in seconds
 */
bool cache_put(
    cache_entry_t *out_entry,
    const char *key,
    const char *content,
    int content_len,
    int ttl
);

/** Initializes a cache_entry_t struct */
void init_cache_entry(cache_entry_t *entry);

/**
 * Append more data to the end of an existing cache entry.
 * 
 * @param entry Cache entry (gotten from cache_put or cache_get)
 * @param content Pointer to the content to write into the cache
 * @param content_len Number of bytes in the content
 */
bool cache_append(cache_entry_t *entry, const char *content, int content_len);

/**
 * Retrieve data from the cache
 * 
 * @param out_entry Cache entry struct which will be filled in
 * @param key Identifier for the cache entry
 * @param out_content_len Output param for the length of the returned content
 * @param out_freeptr Output param for a pointer to free the returned content. Do NOT call free on
 *      the returned content pointer. Will be NULL if NULL is returned.
 * @return Heap-allocated data in the cache, or NULL if the cache doesn't contain the key, or if it
 *      is expired.
 */
char *cache_get(
    cache_entry_t *out_entry,
    const char *key,
    int *out_content_len,
    void **out_freeptr
);

/** Close a cache entry. Use after put or append. */
void cache_close_entry(cache_entry_t *entry);

/**
 * Computes an MD5 hash on the input and outputs the hexadecimal text of the digest.
 * 
 * @param input Input string
 * @return Heap-allocated hex string
 */
char *md5(const char *input);

#endif