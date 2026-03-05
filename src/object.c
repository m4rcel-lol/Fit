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
    if (!full) return -1;

    memcpy(full, header, header_len);
    memcpy(full + header_len, obj->data, obj->size);

    hash_data(full, total, out);

    uLongf compressed_size = compressBound(total);
    unsigned char *compressed = malloc(compressed_size);
    if (!compressed) {
        free(full);
        return -1;
    }

    int compress_result = compress(compressed, &compressed_size, (unsigned char*)full, total);
    if (compress_result != Z_OK) {
        free(full);
        free(compressed);
        return -1;
    }

    char *path = object_path(out);
    if (!path) {
        free(full);
        free(compressed);
        return -1;
    }

    char dir[256];
    snprintf(dir, sizeof(dir), "%s/%.2s", FIT_OBJECTS_DIR, path + strlen(FIT_OBJECTS_DIR) + 1);
    if (mkdirp(dir) != 0) {
        free(full);
        free(compressed);
        free(path);
        return -1;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(full);
        free(compressed);
        free(path);
        return -1;
    }

    size_t written = fwrite(compressed, 1, compressed_size, f);
    if (written != compressed_size) {
        fclose(f);
        free(full);
        free(compressed);
        free(path);
        return -1;
    }

    if (fclose(f) != 0) {
        free(full);
        free(compressed);
        free(path);
        return -1;
    }

    free(full);
    free(compressed);
    free(path);
    return 0;
}

int object_read(const hash_t *hash, object_t *obj) {
    char *path = object_path(hash);
    if (!path) return -1;

    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return -1;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long file_size = ftell(f);
    if (file_size < 0) {
        fclose(f);
        return -1;
    }
    size_t compressed_size = (size_t)file_size;

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    unsigned char *compressed = malloc(compressed_size);
    if (!compressed) {
        fclose(f);
        return -1;
    }

    size_t bytes_read = fread(compressed, 1, compressed_size, f);
    fclose(f);

    if (bytes_read != compressed_size) {
        free(compressed);
        return -1;
    }

    // Use a safer initial size estimate
    uLongf uncompressed_size = compressed_size * 20;  // More conservative estimate
    unsigned char *uncompressed = malloc(uncompressed_size);
    if (!uncompressed) {
        free(compressed);
        return -1;
    }

    int uncompress_result = uncompress(uncompressed, &uncompressed_size, compressed, compressed_size);
    free(compressed);

    if (uncompress_result != Z_OK) {
        free(uncompressed);
        return -1;
    }

    char *type_end = strchr((char*)uncompressed, ' ');
    if (!type_end) {
        free(uncompressed);
        return -1;
    }

    char *size_end = strchr(type_end + 1, '\0');
    if (!size_end) {
        free(uncompressed);
        return -1;
    }

    *type_end = '\0';
    if (strcmp((char*)uncompressed, "blob") == 0) obj->type = OBJ_BLOB;
    else if (strcmp((char*)uncompressed, "tree") == 0) obj->type = OBJ_TREE;
    else if (strcmp((char*)uncompressed, "commit") == 0) obj->type = OBJ_COMMIT;
    else {
        free(uncompressed);
        return -1;
    }

    obj->size = atoll(type_end + 1);
    if (obj->size == 0 && type_end[1] != '0') {
        free(uncompressed);
        return -1;
    }

    obj->data = malloc(obj->size);
    if (!obj->data) {
        free(uncompressed);
        return -1;
    }

    memcpy(obj->data, size_end + 1, obj->size);

    free(uncompressed);
    return 0;
}

void object_free(object_t *obj) {
    if (obj->data) free(obj->data);
}
