#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#include "fit.h"

char* object_path(const hash_t *hash) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(hash, hex);
    
    char *path = malloc(256);
    snprintf(path, 256, "%s/%.2s/%s", FIT_OBJECTS_DIR, hex, hex + 2);
    return path;
}

int object_write(const object_t *obj, hash_t *out) {
    char header[64];
    const char *type_str = obj->type == OBJ_BLOB ? "blob" :
                           obj->type == OBJ_TREE ? "tree" : "commit";
    
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, obj->size);
    header[header_len++] = '\0';
    
    size_t total = header_len + obj->size;
    char *full = malloc(total);
    memcpy(full, header, header_len);
    memcpy(full + header_len, obj->data, obj->size);
    
    hash_data(full, total, out);
    
    uLongf compressed_size = compressBound(total);
    unsigned char *compressed = malloc(compressed_size);
    compress(compressed, &compressed_size, (unsigned char*)full, total);
    
    char *path = object_path(out);
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/%.2s", FIT_OBJECTS_DIR, path + strlen(FIT_OBJECTS_DIR) + 1);
    mkdirp(dir);
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(full);
        free(compressed);
        free(path);
        return -1;
    }
    
    fwrite(compressed, 1, compressed_size, f);
    fclose(f);
    
    free(full);
    free(compressed);
    free(path);
    return 0;
}

int object_read(const hash_t *hash, object_t *obj) {
    char *path = object_path(hash);
    
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    size_t compressed_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    unsigned char *compressed = malloc(compressed_size);
    fread(compressed, 1, compressed_size, f);
    fclose(f);
    
    uLongf uncompressed_size = compressed_size * 10;
    unsigned char *uncompressed = malloc(uncompressed_size);
    uncompress(uncompressed, &uncompressed_size, compressed, compressed_size);
    free(compressed);
    
    char *type_end = strchr((char*)uncompressed, ' ');
    char *size_end = strchr(type_end + 1, '\0');
    
    *type_end = '\0';
    if (strcmp((char*)uncompressed, "blob") == 0) obj->type = OBJ_BLOB;
    else if (strcmp((char*)uncompressed, "tree") == 0) obj->type = OBJ_TREE;
    else if (strcmp((char*)uncompressed, "commit") == 0) obj->type = OBJ_COMMIT;
    
    obj->size = atoll(type_end + 1);
    obj->data = malloc(obj->size);
    memcpy(obj->data, size_end + 1, obj->size);
    
    free(uncompressed);
    return 0;
}

void object_free(object_t *obj) {
    if (obj->data) free(obj->data);
}
