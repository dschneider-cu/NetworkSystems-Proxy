#include "blocklist.h"
#include "../file.h"

vec_t read_blocklist(const char *filepath) {
    int file_size;
    char *file_content = (char *)read_file(filepath, &file_size);
    file_content = strip(file_content, NULL);

    vec_t list = strsplit(file_content, "\n");
    free(file_content);
    return list;
}

void _blocklist_match(int i, const void *data, const void *criteria) {
    return streq_icase((const char *)data, (const char *)criteria);
}

bool blocklist_contains(const vec_t *list, const char host) {
    return vec_indexof(list, _blocklist_match, host) >= 0;
}