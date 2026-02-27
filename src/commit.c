#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fit.h"

int commit_write(const commit_t *commit, hash_t *out) {
    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit->tree, tree_hex);
    hash_to_hex(&commit->parent, parent_hex);
    
    char *data;
    size_t size;
    
    int has_parent = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        if (commit->parent.hash[i]) {
            has_parent = 1;
            break;
        }
    }
    
    if (has_parent) {
        size = asprintf(&data, "tree %s\nparent %s\nauthor %s %ld\n\n%s",
                       tree_hex, parent_hex, commit->author, commit->timestamp, commit->message);
    } else {
        size = asprintf(&data, "tree %s\nauthor %s %ld\n\n%s",
                       tree_hex, commit->author, commit->timestamp, commit->message);
    }
    
    object_t obj = { .data = data, .size = size, .type = OBJ_COMMIT };
    int ret = object_write(&obj, out);
    free(data);
    return ret;
}

int commit_read(const hash_t *hash, commit_t *commit) {
    object_t obj;
    if (object_read(hash, &obj) < 0) return -1;
    
    memset(commit, 0, sizeof(commit_t));
    
    char *line = obj.data;
    char *end = obj.data + obj.size;
    
    while (line < end && *line != '\n') {
        if (strncmp(line, "tree ", 5) == 0) {
            char hex[HASH_HEX_SIZE + 1];
            strncpy(hex, line + 5, HASH_HEX_SIZE);
            hex[HASH_HEX_SIZE] = '\0';
            hex_to_hash(hex, &commit->tree);
        } else if (strncmp(line, "parent ", 7) == 0) {
            char hex[HASH_HEX_SIZE + 1];
            strncpy(hex, line + 7, HASH_HEX_SIZE);
            hex[HASH_HEX_SIZE] = '\0';
            hex_to_hash(hex, &commit->parent);
        } else if (strncmp(line, "author ", 7) == 0) {
            char *timestamp_str = strrchr(line, ' ');
            if (timestamp_str) {
                commit->timestamp = atol(timestamp_str + 1);
                size_t author_len = timestamp_str - (line + 7);
                commit->author = strndup(line + 7, author_len);
            }
        }
        
        line = strchr(line, '\n');
        if (!line) break;
        line++;
        if (*line == '\n') {
            line++;
            break;
        }
    }
    
    if (line < end) {
        commit->message = strndup(line, end - line);
    }
    
    object_free(&obj);
    return 0;
}

void commit_free(commit_t *commit) {
    if (commit->author) free(commit->author);
    if (commit->message) free(commit->message);
}
