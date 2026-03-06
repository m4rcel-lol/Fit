#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fit.h"

// Find the merge base (common ancestor) between two commits
static int find_merge_base(const hash_t *commit1, const hash_t *commit2, hash_t *base) {
    // Simple algorithm: walk back from commit1 and check if any ancestor is reachable from commit2
    hash_t current = *commit1;
    hash_t ancestors[256];
    int ancestor_count = 0;

    // Collect ancestors from commit1
    while (ancestor_count < 256) {
        ancestors[ancestor_count++] = current;

        commit_t commit;
        if (commit_read(&current, &commit) < 0) break;

        int has_parent = 0;
        for (int i = 0; i < HASH_SIZE; i++) {
            if (commit.parent.hash[i]) {
                has_parent = 1;
                break;
            }
        }

        if (!has_parent) {
            commit_free(&commit);
            break;
        }

        current = commit.parent;
        commit_free(&commit);
    }

    // Walk back from commit2 and check if we hit any ancestor from commit1
    current = *commit2;
    for (int depth = 0; depth < 256; depth++) {
        // Check if current is in ancestors list
        for (int i = 0; i < ancestor_count; i++) {
            if (hash_equal(&current, &ancestors[i])) {
                *base = current;
                return 0;
            }
        }

        commit_t commit;
        if (commit_read(&current, &commit) < 0) break;

        int has_parent = 0;
        for (int i = 0; i < HASH_SIZE; i++) {
            if (commit.parent.hash[i]) {
                has_parent = 1;
                break;
            }
        }

        if (!has_parent) {
            commit_free(&commit);
            break;
        }

        current = commit.parent;
        commit_free(&commit);
    }

    fprintf(stderr, "No common ancestor found\n");
    return -1;
}

// Check if commit1 is an ancestor of commit2 (for fast-forward detection)
static int is_ancestor(const hash_t *ancestor, const hash_t *descendant) {
    hash_t current = *descendant;

    for (int depth = 0; depth < 256; depth++) {
        if (hash_equal(&current, ancestor)) {
            return 1;
        }

        commit_t commit;
        if (commit_read(&current, &commit) < 0) break;

        int has_parent = 0;
        for (int i = 0; i < HASH_SIZE; i++) {
            if (commit.parent.hash[i]) {
                has_parent = 1;
                break;
            }
        }

        if (!has_parent) {
            commit_free(&commit);
            break;
        }

        current = commit.parent;
        commit_free(&commit);
    }

    return 0;
}

// Perform three-way merge of a single file
static int merge_file(const hash_t *base_hash, const hash_t *ours_hash,
                      const hash_t *theirs_hash, const char *path) {
    // Read all three versions
    object_t base_obj = {0}, ours_obj = {0}, theirs_obj = {0};
    int has_base = 0, has_ours = 0, has_theirs = 0;

    if (base_hash && !hash_is_null(base_hash)) {
        has_base = (object_read(base_hash, &base_obj) == 0);
    }

    if (ours_hash && !hash_is_null(ours_hash)) {
        has_ours = (object_read(ours_hash, &ours_obj) == 0);
    }

    if (theirs_hash && !hash_is_null(theirs_hash)) {
        has_theirs = (object_read(theirs_hash, &theirs_obj) == 0);
    }

    // Simple merge strategy:
    // 1. If base == ours, use theirs (they changed it)
    // 2. If base == theirs, use ours (we changed it)
    // 3. If ours == theirs, no conflict
    // 4. Otherwise, mark as conflict

    int conflict = 0;

    if (has_base && has_ours && has_theirs) {
        // All three exist - check for changes
        int base_eq_ours = (base_obj.size == ours_obj.size &&
                           memcmp(base_obj.data, ours_obj.data, base_obj.size) == 0);
        int base_eq_theirs = (base_obj.size == theirs_obj.size &&
                             memcmp(base_obj.data, theirs_obj.data, base_obj.size) == 0);
        int ours_eq_theirs = (ours_obj.size == theirs_obj.size &&
                             memcmp(ours_obj.data, theirs_obj.data, ours_obj.size) == 0);

        if (ours_eq_theirs) {
            // Both sides made same change or no change - use either
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(ours_obj.data, 1, ours_obj.size, f);
                fclose(f);
            }
        } else if (base_eq_ours) {
            // We didn't change, they did - use theirs
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(theirs_obj.data, 1, theirs_obj.size, f);
                fclose(f);
            }
        } else if (base_eq_theirs) {
            // They didn't change, we did - use ours
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(ours_obj.data, 1, ours_obj.size, f);
                fclose(f);
            }
        } else {
            // Both sides changed differently - CONFLICT
            conflict = 1;
            FILE *f = fopen(path, "wb");
            if (f) {
                fprintf(f, "<<<<<<< HEAD (ours)\n");
                fwrite(ours_obj.data, 1, ours_obj.size, f);
                fprintf(f, "\n=======\n");
                fwrite(theirs_obj.data, 1, theirs_obj.size, f);
                fprintf(f, "\n>>>>>>> %s (theirs)\n", path);
                fclose(f);
            }
            printf("CONFLICT in %s\n", path);
        }
    } else if (!has_base && has_ours && has_theirs) {
        // File added in both branches
        if (ours_obj.size == theirs_obj.size &&
            memcmp(ours_obj.data, theirs_obj.data, ours_obj.size) == 0) {
            // Same content added - no conflict
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(ours_obj.data, 1, ours_obj.size, f);
                fclose(f);
            }
        } else {
            // Different content - CONFLICT
            conflict = 1;
            FILE *f = fopen(path, "wb");
            if (f) {
                fprintf(f, "<<<<<<< HEAD (ours)\n");
                fwrite(ours_obj.data, 1, ours_obj.size, f);
                fprintf(f, "\n=======\n");
                fwrite(theirs_obj.data, 1, theirs_obj.size, f);
                fprintf(f, "\n>>>>>>> (theirs)\n");
                fclose(f);
            }
            printf("CONFLICT (both added) in %s\n", path);
        }
    } else if (has_base && !has_ours && has_theirs) {
        // We deleted, they modified - CONFLICT
        conflict = 1;
        printf("CONFLICT (deleted by us, modified by them) in %s\n", path);
        // Keep their version
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(theirs_obj.data, 1, theirs_obj.size, f);
            fclose(f);
        }
    } else if (has_base && has_ours && !has_theirs) {
        // They deleted, we modified - CONFLICT
        conflict = 1;
        printf("CONFLICT (modified by us, deleted by them) in %s\n", path);
        // Keep our version
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(ours_obj.data, 1, ours_obj.size, f);
            fclose(f);
        }
    } else if (has_theirs && !has_ours) {
        // They added it - use theirs
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(theirs_obj.data, 1, theirs_obj.size, f);
            fclose(f);
        }
    } else if (has_ours && !has_theirs) {
        // We added it - use ours
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(ours_obj.data, 1, ours_obj.size, f);
            fclose(f);
        }
    }

    if (has_base) object_free(&base_obj);
    if (has_ours) object_free(&ours_obj);
    if (has_theirs) object_free(&theirs_obj);

    return conflict ? -1 : 0;
}

// Recursively merge trees
static int merge_trees_recursive(const hash_t *base_tree, const hash_t *ours_tree,
                                  const hash_t *theirs_tree, const char *prefix) {
    tree_entry_t *base_entries = NULL, *ours_entries = NULL, *theirs_entries = NULL;
    int conflicts = 0;

    // Read all three trees
    if (base_tree && !hash_is_null(base_tree)) {
        base_entries = tree_read(base_tree);
    }
    if (ours_tree && !hash_is_null(ours_tree)) {
        ours_entries = tree_read(ours_tree);
    }
    if (theirs_tree && !hash_is_null(theirs_tree)) {
        theirs_entries = tree_read(theirs_tree);
    }

    // Collect all unique file names
    char **all_names = malloc(sizeof(char*) * 1024);
    if (!all_names) {
        fprintf(stderr, "Failed to allocate memory for file names\n");
        tree_free(base_entries);
        tree_free(ours_entries);
        tree_free(theirs_entries);
        return -1;
    }
    int name_count = 0;

    // Add names from ours
    for (tree_entry_t *e = ours_entries; e; e = e->next) {
        int found = 0;
        for (int i = 0; i < name_count; i++) {
            if (strcmp(all_names[i], e->name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && name_count < 1024) {
            char *name_copy = strdup(e->name);
            if (!name_copy) {
                fprintf(stderr, "Failed to allocate memory for file name\n");
                for (int j = 0; j < name_count; j++) {
                    free(all_names[j]);
                }
                free(all_names);
                tree_free(base_entries);
                tree_free(ours_entries);
                tree_free(theirs_entries);
                return -1;
            }
            all_names[name_count++] = name_copy;
        }
    }

    // Add names from theirs
    for (tree_entry_t *e = theirs_entries; e; e = e->next) {
        int found = 0;
        for (int i = 0; i < name_count; i++) {
            if (strcmp(all_names[i], e->name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && name_count < 1024) {
            char *name_copy = strdup(e->name);
            if (!name_copy) {
                fprintf(stderr, "Failed to allocate memory for file name\n");
                for (int j = 0; j < name_count; j++) {
                    free(all_names[j]);
                }
                free(all_names);
                tree_free(base_entries);
                tree_free(ours_entries);
                tree_free(theirs_entries);
                return -1;
            }
            all_names[name_count++] = name_copy;
        }
    }

    // Add names from base
    for (tree_entry_t *e = base_entries; e; e = e->next) {
        int found = 0;
        for (int i = 0; i < name_count; i++) {
            if (strcmp(all_names[i], e->name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && name_count < 1024) {
            char *name_copy = strdup(e->name);
            if (!name_copy) {
                fprintf(stderr, "Failed to allocate memory for file name\n");
                for (int j = 0; j < name_count; j++) {
                    free(all_names[j]);
                }
                free(all_names);
                tree_free(base_entries);
                tree_free(ours_entries);
                tree_free(theirs_entries);
                return -1;
            }
            all_names[name_count++] = name_copy;
        }
    }

    // Process each file
    for (int i = 0; i < name_count; i++) {
        const char *name = all_names[i];

        // Find entries in each tree
        tree_entry_t *base_entry = NULL, *ours_entry = NULL, *theirs_entry = NULL;

        for (tree_entry_t *e = base_entries; e; e = e->next) {
            if (strcmp(e->name, name) == 0) {
                base_entry = e;
                break;
            }
        }

        for (tree_entry_t *e = ours_entries; e; e = e->next) {
            if (strcmp(e->name, name) == 0) {
                ours_entry = e;
                break;
            }
        }

        for (tree_entry_t *e = theirs_entries; e; e = e->next) {
            if (strcmp(e->name, name) == 0) {
                theirs_entry = e;
                break;
            }
        }

        // Build full path
        char path[512];
        if (prefix && prefix[0]) {
            snprintf(path, sizeof(path), "%s/%s", prefix, name);
        } else {
            snprintf(path, sizeof(path), "%s", name);
        }

        // Check if it's a directory
        int is_dir = (ours_entry && S_ISDIR(ours_entry->mode)) ||
                     (theirs_entry && S_ISDIR(theirs_entry->mode)) ||
                     (base_entry && S_ISDIR(base_entry->mode));

        if (is_dir) {
            // Recursively merge directories
            mkdirp(path);
            hash_t *base_hash = base_entry ? &base_entry->hash : NULL;
            hash_t *ours_hash = ours_entry ? &ours_entry->hash : NULL;
            hash_t *theirs_hash = theirs_entry ? &theirs_entry->hash : NULL;

            if (merge_trees_recursive(base_hash, ours_hash, theirs_hash, path) < 0) {
                conflicts++;
            }
        } else {
            // Merge file
            hash_t *base_hash = base_entry ? &base_entry->hash : NULL;
            hash_t *ours_hash = ours_entry ? &ours_entry->hash : NULL;
            hash_t *theirs_hash = theirs_entry ? &theirs_entry->hash : NULL;

            if (merge_file(base_hash, ours_hash, theirs_hash, path) < 0) {
                conflicts++;
            }
        }
    }

    // Cleanup
    for (int i = 0; i < name_count; i++) {
        free(all_names[i]);
    }
    free(all_names);

    tree_free(base_entries);
    tree_free(ours_entries);
    tree_free(theirs_entries);

    return conflicts > 0 ? -1 : 0;
}

// Main three-way merge function
int merge_three_way(const hash_t *current_commit, const hash_t *target_commit,
                    const char *current_branch, const char *target_branch) {
    // Suppress unused parameter warnings
    (void)current_branch;
    (void)target_branch;

    // Find merge base
    hash_t base;
    if (find_merge_base(current_commit, target_commit, &base) < 0) {
        fprintf(stderr, "Cannot find common ancestor for merge\n");
        return -1;
    }

    char base_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&base, base_hex);
    printf("Merge base: %.8s\n", base_hex);

    // Check for fast-forward
    if (is_ancestor(current_commit, target_commit)) {
        printf("Fast-forward merge possible\n");
        return 1; // Indicate fast-forward is possible
    }

    // Read commits
    commit_t base_commit, ours_commit, theirs_commit;
    if (commit_read(&base, &base_commit) < 0) {
        fprintf(stderr, "Failed to read base commit\n");
        return -1;
    }
    if (commit_read(current_commit, &ours_commit) < 0) {
        fprintf(stderr, "Failed to read current commit\n");
        commit_free(&base_commit);
        return -1;
    }
    if (commit_read(target_commit, &theirs_commit) < 0) {
        fprintf(stderr, "Failed to read target commit\n");
        commit_free(&base_commit);
        commit_free(&ours_commit);
        return -1;
    }

    printf("Performing three-way merge...\n");

    // Merge the trees
    int result = merge_trees_recursive(&base_commit.tree, &ours_commit.tree,
                                       &theirs_commit.tree, NULL);

    commit_free(&base_commit);
    commit_free(&ours_commit);
    commit_free(&theirs_commit);

    if (result < 0) {
        printf("\nAutomatic merge failed; fix conflicts and commit the result.\n");
        return -2; // Indicate conflicts
    }

    printf("Merge successful (no conflicts)\n");
    return 0;
}
