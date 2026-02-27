#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fit.h"

int checkout_tree(const hash_t *tree_hash, const char *prefix) {
    tree_entry_t *entries = tree_read(tree_hash);
    if (!entries) return -1;
    
    for (tree_entry_t *e = entries; e; e = e->next) {
        char path[1024];
        if (prefix && prefix[0]) {
            snprintf(path, sizeof(path), "%s/%s", prefix, e->name);
        } else {
            snprintf(path, sizeof(path), "%s", e->name);
        }
        
        if (S_ISDIR(e->mode)) {
            mkdirp(path);
            checkout_tree(&e->hash, path);
        } else {
            object_t obj;
            if (object_read(&e->hash, &obj) < 0) continue;
            
            char *dir = strdup(path);
            char *last_slash = strrchr(dir, '/');
            if (last_slash) {
                *last_slash = '\0';
                mkdirp(dir);
            }
            free(dir);
            
            write_file(path, obj.data, obj.size);
            chmod(path, e->mode);
            object_free(&obj);
        }
    }
    
    tree_free(entries);
    return 0;
}

int checkout_commit(const hash_t *commit_hash) {
    commit_t commit;
    if (commit_read(commit_hash, &commit) < 0) {
        fprintf(stderr, "Failed to read commit\n");
        return -1;
    }
    
    int ret = checkout_tree(&commit.tree, NULL);
    commit_free(&commit);
    return ret;
}
