#include <stdlib.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "cache.h"
#include "../utils.h"
#include "../file.h"

#define MD5_DIGEST_LENGTH 16 // 128 bit
#define MD5_HEX_LENGTH    32 // 2 hex chars per digest byte
static const char *CACHE_DIR = "cache";


void init_cache_entry(cache_entry_t *entry) {
    entry->filepath = NULL;
    entry->file = NULL;
    entry->file_mode = NULL;
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
    out_entry->file_mode = "wb";
    out_entry->file = open_file(filepath, out_entry->file_mode);
    if (out_entry->file == NULL) {
        // Failed to open file
        cache_close_entry(out_entry);
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

    // Make sure file is opened in append mode
    if (streq(entry->file_mode, "ab")) {
        if (entry->file != NULL) {
            // File is open but wrong mode
            close_file(entry->file);
        }
        // Open file as ab
        entry->file = open_file(entry->filepath, "ab");
        if (entry->file == NULL) {
            // Failed to open file
            return false;
        }
        entry->file_mode = "ab";
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
    return NULL;
}

void cache_close_entry(cache_entry_t *entry) {
    if (entry == NULL) return;
    if (entry->filepath != NULL) free(entry->filepath);
    if (entry->file != NULL) close_file(entry->file);
    entry->filepath = NULL;
    entry->file = NULL;
    entry->file_mode = NULL;
}

bool cache_is_expired(const cache_entry_t *entry) {
    return false;
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