#include <stdlib.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <time.h>
#include "cache.h"
#include "../utils.h"
#include "../file.h"

#define MD5_DIGEST_LENGTH 16 // 128 bit
#define MD5_HEX_LENGTH    32 // 2 hex chars per digest byte
#define TIME_MAX_DIGITS   19 // number of digits in int64
static const char *CACHE_DIR = "cache";


void init_cache_entry(cache_entry_t *entry) {
    entry->filepath = NULL;
    entry->file = NULL;
    entry->file_mode = NULL;
}

/** Ensures the file is open with the given mode */
bool _ensure_entry_mode(cache_entry_t *entry, const char *mode) {
    if (streq(entry->file_mode, mode)) {
        // File already opened with correct mode
        return true;
    } else if (entry->file == NULL) {
        // File is closed
        entry->file = open_file(entry->filepath, mode);
    } else {
        // File is open with wrong mode
        entry->file = reopen_file(entry->file, entry->filepath, mode);
    }
    
    if (entry->file == NULL) {
        // File open/reopen failed
        entry->file_mode = NULL;
        return false;
    }
    entry->file_mode = mode;
    return true;
}

time_t _current_time() {
    time_t now = time(NULL);
    if (now == (time_t)(-1)) {
        pelog("Time error");
        return -1;
    }
    return now;
}

bool cache_put(
    cache_entry_t *out_entry,
    const char *key,
    const char *content,
    int content_len,
    int ttl
) {
    char *hash = md5(key);
    char *filepath = path_join(CACHE_DIR, hash);
    free(hash);

    out_entry->filepath = filepath;
    if (!_ensure_entry_mode(out_entry, "wb")) {
        // Failed to open file
        cache_close_entry(out_entry);
        return false;
    }

    // TTL header buffer. +1 for minus sign, +1 for \n, +1 for \0
    char ttl_header[TIME_MAX_DIGITS + 3];
    time_t expiry = _current_time() + ttl;
    int ttlh_len = snprintf(ttl_header, sizeof(ttl_header), "%ld\n", (int64_t)expiry);
    // Write TTL header
    if (!write_file(out_entry->file, ttl_header, ttlh_len)) {
        return false;
    }

    // Write data
    if (!write_file(out_entry->file, content, content_len)) {
        return false;
    }

    return true;
}

bool cache_append(cache_entry_t *entry, const char *content, int content_len) {
    if (entry->filepath == NULL || (content == NULL && content_len > 0)) return false;
    
    if (!_ensure_entry_mode(entry, "wb")) {
        return false;
    }

    return write_file(entry->file, content, content_len);
}

char *cache_get(
    cache_entry_t *out_entry,
    const char *key,
    int *out_content_len,
    void **out_freeptr
) {
    if (out_content_len != NULL) *out_content_len = 0;
    *out_freeptr = NULL;

    char *hash = md5(key);
    char *filepath = path_join(CACHE_DIR, hash);
    free(hash);
    
    // Open file
    out_entry->filepath = filepath;
    if (!_ensure_entry_mode(out_entry, "rb")) {
        // Failed to open file
        cache_close_entry(out_entry);
        return NULL;
    }
    
    // Read file size
    long file_size = read_file_size(out_entry->file);
    if (file_size == -1) {
        // Failed to read file size
        cache_close_entry(out_entry);
        return NULL;
    }
    
    // Allocate memory for the file content
    char *file_content = (char *)malloc(file_size + 1);
    if (file_content == NULL) {
        elog("Failed to allocate memory: %ld bytes", file_size + 1);
        cache_close_entry(out_entry);
        return NULL;
    }
    
    // Read file content
    if (!read_file_content(out_entry->file, (uint8_t *)file_content, file_size)) {
        // Failed to read file content
        free(file_content);
        cache_close_entry(out_entry);
        return NULL;
    }

    // Get TTL header
    int nl_idx = strnfind(file_content, '\n', file_size);
    if (nl_idx == -1) {
        elog("Invalid cache header: no newline found");
        free(file_content);
        cache_close_entry(out_entry);
        return NULL;
    }
    file_content[nl_idx] = '\0'; // split into two strings

    char *content = file_content + nl_idx + 1; // pointer to content without TTL header
    int expiry;
    if (!strtoint(file_content, &expiry, NULL, false)) {
        elog("Invalid cache header: couldn't parse integer \"%s\"", file_content);
        free(file_content);
        cache_close_entry(out_entry);
        return NULL;
    }

    // Check expiry
    if (_current_time() > expiry) {
        vlog("Cache entry is expired!");
        free(file_content);
        cache_close_entry(out_entry);
        return NULL;
    }

    if (out_content_len != NULL) *out_content_len = file_size;
    *out_freeptr = (void *)file_content;
    return content;
}

void cache_close_entry(cache_entry_t *entry) {
    if (entry == NULL) return;
    if (entry->filepath != NULL) free(entry->filepath);
    if (entry->file != NULL) close_file(entry->file);
    entry->filepath = NULL;
    entry->file = NULL;
    entry->file_mode = NULL;
}

char *md5(const char *input) {
    uint8_t digest[MD5_DIGEST_LENGTH];
    unsigned int digest_len;

    // Compute md5 hash
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    
    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    
    EVP_MD_CTX_free(ctx);

    // Convert digest to hex
    static const char hex_chars[] = "0123456789abcdef";
    char *hex = (char *)malloc(MD5_HEX_LENGTH + 1);

    for (unsigned int i = 0; i < digest_len; i++) {
        hex[i*2]   = hex_chars[(digest[i] >> 4) & 0x0F];
        hex[i*2+1] = hex_chars[digest[i] & 0x0F];
    }
    hex[digest_len * 2] = '\0';
    return hex;
}