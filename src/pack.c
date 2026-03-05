#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <arpa/inet.h>
#include "fit.h"

#define PACK_SIGNATURE "PACK"
#define PACK_VERSION 2

typedef struct {
    uint32_t type;
    uint32_t size;
    hash_t hash;
    void *data;
} pack_entry_t;

int pack_objects(const hash_t *hashes, size_t count, const char *pack_file) {
    FILE *f = fopen(pack_file, "wb");
    if (!f) return -1;

    if (fwrite(PACK_SIGNATURE, 1, 4, f) != 4) {
        fclose(f);
        return -1;
    }

    uint32_t version = htonl(PACK_VERSION);
    if (fwrite(&version, 4, 1, f) != 1) {
        fclose(f);
        return -1;
    }

    uint32_t num_objects = htonl(count);
    if (fwrite(&num_objects, 4, 1, f) != 1) {
        fclose(f);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        object_t obj;
        if (object_read(&hashes[i], &obj) < 0) continue;

        uint32_t type = htonl(obj.type);
        uint32_t size = htonl(obj.size);

        if (fwrite(&type, 4, 1, f) != 1 ||
            fwrite(&size, 4, 1, f) != 1 ||
            fwrite(hashes[i].hash, HASH_SIZE, 1, f) != 1) {
            object_free(&obj);
            fclose(f);
            return -1;
        }

        uLongf compressed_size = compressBound(obj.size);
        unsigned char *compressed = malloc(compressed_size);
        if (!compressed) {
            object_free(&obj);
            fclose(f);
            return -1;
        }

        int compress_result = compress(compressed, &compressed_size, (unsigned char*)obj.data, obj.size);
        if (compress_result != Z_OK) {
            free(compressed);
            object_free(&obj);
            fclose(f);
            return -1;
        }

        uint32_t comp_size = htonl(compressed_size);
        if (fwrite(&comp_size, 4, 1, f) != 1 ||
            fwrite(compressed, compressed_size, 1, f) != 1) {
            free(compressed);
            object_free(&obj);
            fclose(f);
            return -1;
        }

        free(compressed);
        object_free(&obj);
    }

    if (fclose(f) != 0) {
        return -1;
    }

    return 0;
}

int unpack_objects(const char *pack_file) {
    FILE *f = fopen(pack_file, "rb");
    if (!f) return -1;

    char sig[4];
    if (fread(sig, 1, 4, f) != 4) {
        fclose(f);
        return -1;
    }

    if (memcmp(sig, PACK_SIGNATURE, 4) != 0) {
        fclose(f);
        return -1;
    }

    uint32_t version, num_objects;
    if (fread(&version, 4, 1, f) != 1 || fread(&num_objects, 4, 1, f) != 1) {
        fclose(f);
        return -1;
    }

    num_objects = ntohl(num_objects);

    for (uint32_t i = 0; i < num_objects; i++) {
        uint32_t type, size;
        hash_t hash;

        if (fread(&type, 4, 1, f) != 1 ||
            fread(&size, 4, 1, f) != 1 ||
            fread(hash.hash, HASH_SIZE, 1, f) != 1) {
            fclose(f);
            return -1;
        }

        type = ntohl(type);
        size = ntohl(size);

        uint32_t comp_size;
        if (fread(&comp_size, 4, 1, f) != 1) {
            fclose(f);
            return -1;
        }
        comp_size = ntohl(comp_size);

        unsigned char *compressed = malloc(comp_size);
        if (!compressed) {
            fclose(f);
            return -1;
        }

        if (fread(compressed, comp_size, 1, f) != 1) {
            free(compressed);
            fclose(f);
            return -1;
        }

        uLongf uncompressed_size = size;
        unsigned char *uncompressed = malloc(uncompressed_size);
        if (!uncompressed) {
            free(compressed);
            fclose(f);
            return -1;
        }

        int uncompress_result = uncompress(uncompressed, &uncompressed_size, compressed, comp_size);
        if (uncompress_result != Z_OK) {
            free(compressed);
            free(uncompressed);
            fclose(f);
            return -1;
        }

        object_t obj = { .data = (char*)uncompressed, .size = size, .type = type };
        hash_t verify_hash;
        if (object_write(&obj, &verify_hash) < 0) {
            free(compressed);
            free(uncompressed);
            fclose(f);
            return -1;
        }

        free(compressed);
        free(uncompressed);
    }

    fclose(f);
    return 0;
}
