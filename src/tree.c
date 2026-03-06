#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fit.h"

tree_entry_t* tree_entry_new(uint32_t mode, const char *name, const hash_t *hash) {
    tree_entry_t *entry = calloc(1, sizeof(tree_entry_t));
    if (!entry) {
        fprintf(stderr, "Failed to allocate memory for tree entry\n");
        return NULL;
    }
    entry->mode = mode;
    entry->name = strdup(name);
    if (!entry->name) {
        fprintf(stderr, "Failed to duplicate name for tree entry\n");
        free(entry);
        return NULL;
    }
    entry->hash = *hash;
    return entry;
}

int tree_write(tree_entry_t *entries, hash_t *out) {
    size_t size = 0;
    for (tree_entry_t *e = entries; e; e = e->next) {
        size += snprintf(NULL, 0, "%o %s", e->mode, e->name) + 1 + HASH_SIZE;
    }

    char *data = malloc(size);
    if (!data) {
        fprintf(stderr, "Failed to allocate memory for tree\n");
        return -1;
    }
    char *ptr = data;
    char *end = data + size;

    for (tree_entry_t *e = entries; e; e = e->next) {
        int written = snprintf(ptr, end - ptr, "%o %s", e->mode, e->name);
        if (written < 0 || ptr + written >= end) {
            fprintf(stderr, "Buffer overflow in tree_write\n");
            free(data);
            return -1;
        }
        ptr += written;
        *ptr++ = '\0';
        if (ptr + HASH_SIZE > end) {
            fprintf(stderr, "Buffer overflow in tree_write\n");
            free(data);
            return -1;
        }
        memcpy(ptr, e->hash.hash, HASH_SIZE);
        ptr += HASH_SIZE;
    }

    object_t obj = { .data = data, .size = size, .type = OBJ_TREE };
    int ret = object_write(&obj, out);
    free(data);
    return ret;
}

tree_entry_t* tree_read(const hash_t *hash) {
    object_t obj;
    if (object_read(hash, &obj) < 0) return NULL;
    
    tree_entry_t *head = NULL, *tail = NULL;
    char *ptr = obj.data;
    char *end = obj.data + obj.size;
    
    while (ptr < end) {
        uint32_t mode = strtol(ptr, &ptr, 8);
        ptr++;
        char *name = ptr;
        ptr += strlen(name) + 1;
        
        hash_t entry_hash;
        memcpy(entry_hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;
        
        tree_entry_t *entry = tree_entry_new(mode, name, &entry_hash);
        if (!head) head = entry;
        if (tail) tail->next = entry;
        tail = entry;
    }
    
    object_free(&obj);
    return head;
}

void tree_free(tree_entry_t *entries) {
    while (entries) {
        tree_entry_t *next = entries->next;
        free(entries->name);
        free(entries);
        entries = next;
    }
}
