#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "fit.h"

static int verify_object(const hash_t *hash) {
    object_t obj;
    if (object_read(hash, &obj) < 0) {
        return -1;
    }

    // Verify hash integrity by re-computing
    char header[64];
    const char *type_str = obj.type == OBJ_BLOB ? "blob" :
                           obj.type == OBJ_TREE ? "tree" : "commit";

    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, obj.size);
    header[header_len++] = '\0';

    size_t total = header_len + obj.size;
    char *full = malloc(total);
    if (!full) {
        object_free(&obj);
        return -1;
    }

    memcpy(full, header, header_len);
    memcpy(full + header_len, obj.data, obj.size);

    hash_t computed_hash;
    hash_data(full, total, &computed_hash);

    free(full);
    object_free(&obj);

    if (!hash_equal(hash, &computed_hash)) {
        return -1;
    }

    return 0;
}

static int collect_all_objects(hash_t **hashes, size_t *count) {
    size_t capacity = 1024;
    *count = 0;
    *hashes = malloc(capacity * sizeof(hash_t));
    if (!*hashes) return -1;

    DIR *dir = opendir(FIT_OBJECTS_DIR);
    if (!dir) {
        free(*hashes);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strlen(entry->d_name) != 2) continue;

        char subdir[512];
        snprintf(subdir, sizeof(subdir), "%s/%s", FIT_OBJECTS_DIR, entry->d_name);

        DIR *subdir_dir = opendir(subdir);
        if (!subdir_dir) continue;

        struct dirent *obj_entry;
        while ((obj_entry = readdir(subdir_dir)) != NULL) {
            if (obj_entry->d_name[0] == '.') continue;

            char hex[512];
            snprintf(hex, sizeof(hex), "%s%s", entry->d_name, obj_entry->d_name);

            if (strlen(hex) != HASH_HEX_SIZE) continue;

            if (*count >= capacity) {
                capacity *= 2;
                hash_t *new_hashes = realloc(*hashes, capacity * sizeof(hash_t));
                if (!new_hashes) {
                    closedir(subdir_dir);
                    closedir(dir);
                    free(*hashes);
                    return -1;
                }
                *hashes = new_hashes;
            }

            if (hex_to_hash(hex, &(*hashes)[*count]) == 0) {
                (*count)++;
            }
        }
        closedir(subdir_dir);
    }
    closedir(dir);

    return 0;
}

int verify_repository(void) {
    printf("Verifying repository integrity...\n");

    hash_t *hashes = NULL;
    size_t count = 0;

    if (collect_all_objects(&hashes, &count) < 0) {
        fprintf(stderr, "Error: Failed to collect objects\n");
        return -1;
    }

    printf("Found %zu objects to verify\n", count);

    size_t verified = 0;
    size_t corrupted = 0;

    for (size_t i = 0; i < count; i++) {
        if (verify_object(&hashes[i]) == 0) {
            verified++;
        } else {
            char hex[HASH_HEX_SIZE + 1];
            hash_to_hex(&hashes[i], hex);
            fprintf(stderr, "Error: Object %s is corrupted\n", hex);
            corrupted++;
        }

        // Show progress every 100 objects
        if ((i + 1) % 100 == 0) {
            printf("Progress: %zu/%zu objects verified\n", i + 1, count);
        }
    }

    free(hashes);

    printf("\nVerification complete:\n");
    printf("  Verified: %zu objects\n", verified);
    printf("  Corrupted: %zu objects\n", corrupted);

    if (corrupted > 0) {
        fprintf(stderr, "\nWarning: Repository contains corrupted objects!\n");
        return -1;
    }

    printf("\nRepository integrity check passed!\n");
    return 0;
}
