#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fit.h"

int ref_read(const char *name, hash_t *hash) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FIT_REFS_DIR, name);
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char hex[HASH_HEX_SIZE + 1];
    if (!fgets(hex, sizeof(hex), f)) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return hex_to_hash(hex, hash);
}

int ref_write(const char *name, const hash_t *hash) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FIT_REFS_DIR, name);
    
    char dir[512];
    strncpy(dir, path, sizeof(dir));
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdirp(dir);
    }
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(hash, hex);
    fprintf(f, "%s\n", hex);
    
    fclose(f);
    return 0;
}

char* ref_current_branch(void) {
    FILE *f = fopen(FIT_HEAD_FILE, "r");
    if (!f) return NULL;
    
    char line[512];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    
    if (strncmp(line, "ref: refs/heads/", 16) == 0) {
        line[strcspn(line, "\n")] = 0;
        return strdup(line + 16);
    }
    
    return NULL;
}

int ref_resolve_head(hash_t *hash) {
    FILE *f = fopen(FIT_HEAD_FILE, "r");
    if (!f) return -1;
    
    char line[512];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    line[strcspn(line, "\n")] = 0;
    
    if (strncmp(line, "ref: ", 5) == 0) {
        char ref_path[512];
        snprintf(ref_path, sizeof(ref_path), "%s/%s", FIT_DIR, line + 5);
        
        f = fopen(ref_path, "r");
        if (!f) return -1;
        
        char hex[HASH_HEX_SIZE + 1];
        if (!fgets(hex, sizeof(hex), f)) {
            fclose(f);
            return -1;
        }
        fclose(f);
        
        hex[strcspn(hex, "\n")] = 0;
        return hex_to_hash(hex, hash);
    }
    
    return hex_to_hash(line, hash);
}

int ref_update_head(const hash_t *hash) {
    char *branch = ref_current_branch();
    if (branch) {
        char ref_name[512];
        snprintf(ref_name, sizeof(ref_name), "heads/%s", branch);
        free(branch);
        return ref_write(ref_name, hash);
    }
    
    FILE *f = fopen(FIT_HEAD_FILE, "w");
    if (!f) return -1;
    
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(hash, hex);
    fprintf(f, "%s\n", hex);
    
    fclose(f);
    return 0;
}
