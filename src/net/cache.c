#include <stdlib.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "cache.h"
#include "../utils.h"
#include "../file.h"

#define MD5_DIGEST_LENGTH 16 // 128 bit
#define MD5_HEX_LENGTH    32 // 2 hex chars per digest byte


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