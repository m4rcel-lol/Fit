#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fit.h"

static int mark_reachable(const hash_t *hash, int *marked, hash_t *all_objects, int count);

static int collect_objects(const char *dir, hash_t **objects, int *count, int *capacity) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (entry->d_name[0] == '.') continue;
        
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        
        struct stat st;
        if (stat(path, &st) < 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            collect_objects(path, objects, count, capacity);
        } else {
            if (*count >= *capacity) {
                *capacity *= 2;
                *objects = realloc(*objects, *capacity * sizeof(hash_t));
            }
            
            char hex[HASH_HEX_SIZE + 1];
            const char *parent = strrchr(dir, '/');
            snprintf(hex, sizeof(hex), "%s%s", parent ? parent + 1 : "", entry->d_name);
            hex_to_hash(hex, &(*objects)[*count]);
            (*count)++;
        }
    }
    
    closedir(d);
    return 0;
}

static int mark_reachable(const hash_t *hash, int *marked, hash_t *all_objects, int count) {
    for (int i = 0; i < count; i++) {
        if (hash_equal(hash, &all_objects[i])) {
            if (marked[i]) return 0;
            marked[i] = 1;
            break;
        }
    }
    
    object_t obj;
    if (object_read(hash, &obj) < 0) return 0;
    
    if (obj.type == OBJ_COMMIT) {
        commit_t commit;
        commit_read(hash, &commit);
        mark_reachable(&commit.tree, marked, all_objects, count);
        
        int has_parent = 0;
        for (int i = 0; i < HASH_SIZE; i++) {
            if (commit.parent.hash[i]) {
                has_parent = 1;
                break;
            }
        }
        if (has_parent) {
            mark_reachable(&commit.parent, marked, all_objects, count);
        }
        commit_free(&commit);
    } else if (obj.type == OBJ_TREE) {
        tree_entry_t *entries = tree_read(hash);
        for (tree_entry_t *e = entries; e; e = e->next) {
            mark_reachable(&e->hash, marked, all_objects, count);
        }
        tree_free(entries);
    }
    
    object_free(&obj);
    return 0;
}

int gc_run(void) {
    int capacity = 1024;
    int count = 0;
    hash_t *objects = malloc(capacity * sizeof(hash_t));
    
    collect_objects(FIT_OBJECTS_DIR, &objects, &count, &capacity);
    
    int *marked = calloc(count, sizeof(int));
    
    DIR *d = opendir(FIT_HEADS_DIR);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d))) {
            if (entry->d_name[0] == '.') continue;
            
            hash_t ref_hash;
            char ref_name[256];
            snprintf(ref_name, sizeof(ref_name), "heads/%s", entry->d_name);
            if (ref_read(ref_name, &ref_hash) == 0) {
                mark_reachable(&ref_hash, marked, objects, count);
            }
        }
        closedir(d);
    }
    
    int removed = 0;
    for (int i = 0; i < count; i++) {
        if (!marked[i]) {
            char *path = object_path(&objects[i]);
            unlink(path);
            free(path);
            removed++;
        }
    }
    
    free(objects);
    free(marked);
    
    printf("Removed %d unreachable objects\n", removed);
    return 0;
}
