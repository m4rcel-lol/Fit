#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fit.h"

int index_read(index_entry_t **entries) {
    *entries = NULL;
    
    FILE *f = fopen(FIT_INDEX_FILE, "r");
    if (!f) return 0;
    
    index_entry_t *head = NULL, *tail = NULL;
    char line[4096];
    
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        
        char hash_hex[HASH_HEX_SIZE + 1];
        uint32_t mode;
        char path[2048];
        
        if (sscanf(line, "%o %64s %2047s", &mode, hash_hex, path) != 3) continue;

        index_entry_t *entry = calloc(1, sizeof(index_entry_t));
        if (!entry) {
            fprintf(stderr, "Failed to allocate memory for index entry\n");
            continue;
        }
        entry->mode = mode;
        entry->path = strdup(path);
        if (!entry->path) {
            fprintf(stderr, "Failed to duplicate path for index entry\n");
            free(entry);
            continue;
        }
        hex_to_hash(hash_hex, &entry->hash);
        
        if (!head) head = entry;
        if (tail) tail->next = entry;
        tail = entry;
    }
    
    fclose(f);
    *entries = head;
    return 0;
}

int index_write(index_entry_t *entries) {
    FILE *f = fopen(FIT_INDEX_FILE, "w");
    if (!f) return -1;
    
    for (index_entry_t *e = entries; e; e = e->next) {
        char hash_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&e->hash, hash_hex);
        fprintf(f, "%o %s %s\n", e->mode, hash_hex, e->path);
    }
    
    fclose(f);
    return 0;
}

int index_add(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) return -1;
    
    size_t size;
    char *data = read_file(path, &size);
    if (!data) return -1;
    
    object_t obj = { .data = data, .size = size, .type = OBJ_BLOB };
    hash_t hash;
    object_write(&obj, &hash);
    free(data);
    
    index_entry_t *entries;
    index_read(&entries);
    
    for (index_entry_t *e = entries; e; e = e->next) {
        if (strcmp(e->path, path) == 0) {
            e->hash = hash;
            e->mode = st.st_mode;
            index_write(entries);
            index_free(entries);
            return 0;
        }
    }

    index_entry_t *new_entry = calloc(1, sizeof(index_entry_t));
    if (!new_entry) {
        fprintf(stderr, "Failed to allocate memory for index entry\n");
        index_free(entries);
        return -1;
    }
    new_entry->path = strdup(path);
    if (!new_entry->path) {
        fprintf(stderr, "Failed to duplicate path for index entry\n");
        free(new_entry);
        index_free(entries);
        return -1;
    }
    new_entry->hash = hash;
    new_entry->mode = st.st_mode;
    
    /* Append to end of list */
    if (!entries) {
        entries = new_entry;
    } else {
        index_entry_t *tail = entries;
        while (tail->next) tail = tail->next;
        tail->next = new_entry;
    }
    
    index_write(entries);
    index_free(entries);
    return 0;
}

int index_remove(const char *path) {
    index_entry_t *entries;
    index_read(&entries);

    if (!entries) {
        fprintf(stderr, "Index is empty\n");
        return -1;
    }

    index_entry_t *prev = NULL;
    for (index_entry_t *e = entries; e; prev = e, e = e->next) {
        if (strcmp(e->path, path) == 0) {
            if (prev) {
                prev->next = e->next;
            } else {
                entries = e->next;
            }
            e->next = NULL;
            index_free(e);
            index_write(entries);
            index_free(entries);
            return 0;
        }
    }

    fprintf(stderr, "File '%s' not found in index\n", path);
    index_free(entries);
    return -1;
}

void index_free(index_entry_t *entries) {
    while (entries) {
        index_entry_t *next = entries->next;
        free(entries->path);
        free(entries);
        entries = next;
    }
}
