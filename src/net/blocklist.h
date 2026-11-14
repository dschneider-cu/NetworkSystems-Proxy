#ifndef BLOCKLIST_H
#define BLOCKLIST_H

#include "../utils.h"

/**
 * Reads the blocklist at the file and returns a vec of blocked host strings.
 */
vec_t read_blocklist(const char *filepath);

/** 
 * Returns true if the blocklist contains the host.
 * 
 * @param list Blocklist (gotten from read_blocklist)
 * @param host Host string (domain name or IP string)
 */
bool blocklist_contains(const vec_t *list, const char host);

#endif