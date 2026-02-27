#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "fit.h"

void hash_data(const void *data, size_t len, hash_t *out) {
    SHA256((const unsigned char*)data, len, out->hash);
}

void hash_to_hex(const hash_t *hash, char *hex) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex + i * 2, "%02x", hash->hash[i]);
    }
    hex[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, hash_t *hash) {
    if (strlen(hex) != HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        sscanf(hex + i * 2, "%2hhx", &hash->hash[i]);
    }
    return 0;
}

int hash_equal(const hash_t *a, const hash_t *b) {
    return memcmp(a->hash, b->hash, HASH_SIZE) == 0;
}
