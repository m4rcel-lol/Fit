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
    
    fwrite(PACK_SIGNATURE, 1, 4, f);
    uint32_t version = htonl(PACK_VERSION);
    fwrite(&version, 4, 1, f);
    uint32_t num_objects = htonl(count);
    fwrite(&num_objects, 4, 1, f);
    
    for (size_t i = 0; i < count; i++) {
        object_t obj;
        if (object_read(&hashes[i], &obj) < 0) continue;
        
        uint32_t type = htonl(obj.type);
        uint32_t size = htonl(obj.size);
        
        fwrite(&type, 4, 1, f);
        fwrite(&size, 4, 1, f);
        fwrite(hashes[i].hash, HASH_SIZE, 1, f);
        
        uLongf compressed_size = compressBound(obj.size);
        unsigned char *compressed = malloc(compressed_size);
        compress(compressed, &compressed_size, (unsigned char*)obj.data, obj.size);
        
        uint32_t comp_size = htonl(compressed_size);
        fwrite(&comp_size, 4, 1, f);
        fwrite(compressed, compressed_size, 1, f);
        
        free(compressed);
        object_free(&obj);
    }
    
    fclose(f);
    return 0;
}

int unpack_objects(const char *pack_file) {
    FILE *f = fopen(pack_file, "rb");
    if (!f) return -1;
    
    char sig[4];
    fread(sig, 1, 4, f);
    if (memcmp(sig, PACK_SIGNATURE, 4) != 0) {
        fclose(f);
        return -1;
    }
    
    uint32_t version, num_objects;
    fread(&version, 4, 1, f);
    fread(&num_objects, 4, 1, f);
    num_objects = ntohl(num_objects);
    
    for (uint32_t i = 0; i < num_objects; i++) {
        uint32_t type, size;
        hash_t hash;
        
        fread(&type, 4, 1, f);
        fread(&size, 4, 1, f);
        fread(hash.hash, HASH_SIZE, 1, f);
        
        type = ntohl(type);
        size = ntohl(size);
        
        uint32_t comp_size;
        fread(&comp_size, 4, 1, f);
        comp_size = ntohl(comp_size);
        
        unsigned char *compressed = malloc(comp_size);
        fread(compressed, comp_size, 1, f);
        
        uLongf uncompressed_size = size;
        unsigned char *uncompressed = malloc(uncompressed_size);
        uncompress(uncompressed, &uncompressed_size, compressed, comp_size);
        
        object_t obj = { .data = (char*)uncompressed, .size = size, .type = type };
        hash_t verify_hash;
        object_write(&obj, &verify_hash);
        
        free(compressed);
        free(uncompressed);
    }
    
    fclose(f);
    return 0;
}
