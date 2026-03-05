#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "fit.h"

#define FIT_TAGS_DIR ".fit/refs/tags"

int tag_create(const char *name, const hash_t *commit_hash, const char *message) {
    char tag_path[512];
    snprintf(tag_path, sizeof(tag_path), "%s/%s", FIT_TAGS_DIR, name);

    /* Create tags directory if it doesn't exist */
    mkdirp(FIT_TAGS_DIR);

    /* Check if tag already exists */
    if (file_exists(tag_path)) {
        fprintf(stderr, "Tag '%s' already exists\n", name);
        return -1;
    }

    /* Write tag file with commit hash and optional message */
    FILE *f = fopen(tag_path, "w");
    if (!f) {
        fprintf(stderr, "Failed to create tag file\n");
        return -1;
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(commit_hash, hex);
    fprintf(f, "%s\n", hex);

    if (message && message[0]) {
        fprintf(f, "%s\n", message);
    }

    fclose(f);
    return 0;
}

int tag_delete(const char *name) {
    char tag_path[512];
    snprintf(tag_path, sizeof(tag_path), "%s/%s", FIT_TAGS_DIR, name);

    if (!file_exists(tag_path)) {
        fprintf(stderr, "Tag '%s' not found\n", name);
        return -1;
    }

    if (remove(tag_path) < 0) {
        fprintf(stderr, "Failed to delete tag '%s'\n", name);
        return -1;
    }

    return 0;
}

int tag_list(void) {
    DIR *d = opendir(FIT_TAGS_DIR);
    if (!d) {
        return 0;  /* No tags directory means no tags */
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(d))) {
        if (entry->d_name[0] == '.') continue;

        /* Read tag info */
        char tag_path[512];
        snprintf(tag_path, sizeof(tag_path), "%s/%s", FIT_TAGS_DIR, entry->d_name);

        FILE *f = fopen(tag_path, "r");
        if (!f) continue;

        char hash_hex[HASH_HEX_SIZE + 1];
        char message[256] = {0};

        if (fgets(hash_hex, sizeof(hash_hex), f)) {
            /* Remove newline */
            hash_hex[strcspn(hash_hex, "\n")] = 0;

            /* Try to read message */
            if (fgets(message, sizeof(message), f)) {
                message[strcspn(message, "\n")] = 0;
            }
        }
        fclose(f);

        if (message[0]) {
            printf("%-20s %.8s  %s\n", entry->d_name, hash_hex, message);
        } else {
            printf("%-20s %.8s\n", entry->d_name, hash_hex);
        }
        count++;
    }

    closedir(d);
    return count;
}

int tag_resolve(const char *name, hash_t *out) {
    char tag_path[512];
    snprintf(tag_path, sizeof(tag_path), "%s/%s", FIT_TAGS_DIR, name);

    if (!file_exists(tag_path)) {
        return -1;
    }

    FILE *f = fopen(tag_path, "r");
    if (!f) return -1;

    char hash_hex[HASH_HEX_SIZE + 1];
    if (!fgets(hash_hex, sizeof(hash_hex), f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    /* Remove newline */
    hash_hex[strcspn(hash_hex, "\n")] = 0;

    return hex_to_hash(hash_hex, out);
}
