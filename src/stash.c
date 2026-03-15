#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "fit.h"

#define FIT_STASH_DIR ".fit/stash"
#define FIT_STASH_INDEX ".fit/stash/index"

int stash_save(const char *message) {
    /* Create stash directory if needed */
    mkdirp(FIT_STASH_DIR);

    /* Read current index */
    index_entry_t *entries;
    index_read(&entries);

    if (!entries) {
        fprintf(stderr, "Nothing to stash (no staged changes)\n");
        return -1;
    }

    /* Generate stash ID based on timestamp */
    time_t now = time(NULL);
    char stash_id[32];
    snprintf(stash_id, sizeof(stash_id), "stash@%ld", (long)now);

    /* Create stash commit */
    hash_t tree_hash;
    tree_entry_t *tree_head = NULL, *tree_tail = NULL;

    for (index_entry_t *e = entries; e; e = e->next) {
        tree_entry_t *te = tree_entry_new(e->mode, e->path, &e->hash);
        if (!te) {
            fprintf(stderr, "Error: Failed to allocate tree entry for stash\n");
            tree_free(tree_head);
            index_free(entries);
            return -1;
        }
        if (!tree_head) tree_head = te;
        if (tree_tail) tree_tail->next = te;
        tree_tail = te;
    }

    tree_write(tree_head, &tree_hash);
    tree_free(tree_head);

    /* Create commit object for stash */
    commit_t commit = {0};
    commit.tree = tree_hash;

    /* Try to get parent from HEAD */
    hash_t parent_hash;
    if (ref_resolve_head(&parent_hash) == 0) {
        commit.parent = parent_hash;
    }

    char *user = getenv("USER");
    if (!user) user = "unknown";
    commit.author = user;
    commit.message = message ? (char *)message : "WIP on branch";
    commit.timestamp = now;

    hash_t commit_hash;
    commit_write(&commit, &commit_hash);

    /* Save stash reference */
    char stash_ref_path[512];
    snprintf(stash_ref_path, sizeof(stash_ref_path), "%s/%s", FIT_STASH_DIR, stash_id);

    FILE *f = fopen(stash_ref_path, "w");
    if (!f) {
        fprintf(stderr, "Failed to save stash\n");
        index_free(entries);
        return -1;
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_hash, hex);
    fprintf(f, "%s\n", hex);
    if (message) {
        fprintf(f, "%s\n", message);
    }
    fclose(f);

    /* Clear the index */
    FILE *idx = fopen(FIT_INDEX_FILE, "w");
    if (idx) fclose(idx);

    index_free(entries);
    printf("Saved working directory to %s\n", stash_id);

    return 0;
}

int stash_list(void) {
    DIR *d = opendir(FIT_STASH_DIR);
    if (!d) {
        return 0;  /* No stash directory means no stashes */
    }

    struct dirent *entry;
    int count = 0;

    printf("Stash list:\n");

    while ((entry = readdir(d))) {
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "index") == 0) continue;

        char stash_path[512];
        snprintf(stash_path, sizeof(stash_path), "%s/%s", FIT_STASH_DIR, entry->d_name);

        FILE *f = fopen(stash_path, "r");
        if (!f) continue;

        char hash_hex[HASH_HEX_SIZE + 1];
        char message[256] = {0};

        if (fgets(hash_hex, sizeof(hash_hex), f)) {
            hash_hex[strcspn(hash_hex, "\n")] = 0;

            if (fgets(message, sizeof(message), f)) {
                message[strcspn(message, "\n")] = 0;
            }
        }
        fclose(f);

        printf("  %s: %s %.8s\n", entry->d_name,
               message[0] ? message : "WIP", hash_hex);
        count++;
    }

    closedir(d);

    if (count == 0) {
        printf("  (empty)\n");
    }

    return count;
}

int stash_pop(const char *stash_name) {
    char stash_path[512];

    if (stash_name) {
        /* Validate stash name to prevent directory traversal */
        if (!is_valid_ref_name(stash_name)) {
            fprintf(stderr, "Error: Invalid stash name '%s'\n", stash_name);
            return -1;
        }
        snprintf(stash_path, sizeof(stash_path), "%s/%s", FIT_STASH_DIR, stash_name);
    } else {
        /* Find the most recent stash */
        DIR *d = opendir(FIT_STASH_DIR);
        if (!d) {
            fprintf(stderr, "No stash entries found\n");
            return -1;
        }

        struct dirent *entry;
        time_t latest_time = 0;
        char latest_stash[256] = {0};

        while ((entry = readdir(d))) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, "index") == 0) continue;

            /* Parse timestamp from stash name */
            long timestamp;
            if (sscanf(entry->d_name, "stash@%ld", &timestamp) == 1) {
                if (timestamp > latest_time) {
                    latest_time = timestamp;
                    strncpy(latest_stash, entry->d_name, sizeof(latest_stash) - 1);
                    latest_stash[sizeof(latest_stash) - 1] = '\0';
                }
            }
        }
        closedir(d);

        if (latest_stash[0] == 0) {
            fprintf(stderr, "No stash entries found\n");
            return -1;
        }

        snprintf(stash_path, sizeof(stash_path), "%s/%s", FIT_STASH_DIR, latest_stash);
    }

    if (!file_exists(stash_path)) {
        fprintf(stderr, "Stash not found\n");
        return -1;
    }

    /* Read stash commit hash */
    FILE *f = fopen(stash_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to read stash\n");
        return -1;
    }

    char hash_hex[HASH_HEX_SIZE + 1];
    if (!fgets(hash_hex, sizeof(hash_hex), f)) {
        fclose(f);
        fprintf(stderr, "Invalid stash format\n");
        return -1;
    }
    fclose(f);

    hash_hex[strcspn(hash_hex, "\n")] = 0;

    /* Convert to hash */
    hash_t commit_hash;
    if (hex_to_hash(hash_hex, &commit_hash) < 0) {
        fprintf(stderr, "Invalid stash hash\n");
        return -1;
    }

    /* Restore the stash by checking out its tree */
    commit_t commit;
    if (commit_read(&commit_hash, &commit) < 0) {
        fprintf(stderr, "Failed to read stash commit\n");
        return -1;
    }

    if (checkout_tree(&commit.tree, NULL) < 0) {
        commit_free(&commit);
        fprintf(stderr, "Failed to restore stash\n");
        return -1;
    }

    commit_free(&commit);

    /* Remove the stash */
    remove(stash_path);

    printf("Applied and removed stash\n");
    return 0;
}

int stash_drop(const char *stash_name) {
    char stash_path[512];

    if (!stash_name) {
        fprintf(stderr, "Usage: fit stash drop <stash-name>\n");
        return -1;
    }

    /* Validate stash name to prevent directory traversal */
    if (!is_valid_ref_name(stash_name)) {
        fprintf(stderr, "Error: Invalid stash name '%s'\n", stash_name);
        return -1;
    }

    snprintf(stash_path, sizeof(stash_path), "%s/%s", FIT_STASH_DIR, stash_name);

    if (!file_exists(stash_path)) {
        fprintf(stderr, "Stash '%s' not found\n", stash_name);
        return -1;
    }

    if (remove(stash_path) < 0) {
        fprintf(stderr, "Failed to remove stash\n");
        return -1;
    }

    printf("Dropped %s\n", stash_name);
    return 0;
}
