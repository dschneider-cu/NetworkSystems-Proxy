#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>

bool cache_put();

char *cache_get();

/**
 * Computes an MD5 hash on the input and outputs the hexadecimal text of the digest.
 * 
 * @param input Input string
 * @return Heap-allocated hex string
 */
char *md5(const char *input);

#endif