#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fit.h"

#define FIT_SHALLOW_FILE ".fit/shallow"

/* Mark repository as shallow */
int shallow_mark(const hash_t *shallow_commits, size_t count) {
    FILE *f = fopen(FIT_SHALLOW_FILE, "w");
    if (!f) {
        fprintf(stderr, "Failed to create shallow file\n");
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&shallow_commits[i], hex);
        fprintf(f, "%s\n", hex);
    }

    fclose(f);
    return 0;
}

/* Check if repository is shallow */
int shallow_is_repository_shallow(void) {
    FILE *f = fopen(FIT_SHALLOW_FILE, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* Get list of shallow commits */
int shallow_read_commits(hash_t **commits_out, size_t *count_out) {
    FILE *f = fopen(FIT_SHALLOW_FILE, "r");
    if (!f) {
        *commits_out = NULL;
        *count_out = 0;
        return 0;
    }

    size_t capacity = 16;
    hash_t *commits = malloc(capacity * sizeof(hash_t));
    if (!commits) {
        fclose(f);
        return -1;
    }

    size_t count = 0;
    char line[HASH_HEX_SIZE + 2];

    while (fgets(line, sizeof(line), f)) {
        /* Remove newline */
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) != HASH_HEX_SIZE) continue;

        if (count >= capacity) {
            capacity *= 2;
            hash_t *new_commits = realloc(commits, capacity * sizeof(hash_t));
            if (!new_commits) {
                free(commits);
                fclose(f);
                return -1;
            }
            commits = new_commits;
        }

        if (hex_to_hash(line, &commits[count]) == 0) {
            count++;
        }
    }

    fclose(f);
    *commits_out = commits;
    *count_out = count;
    return 0;
}

/* Check if a commit is a shallow boundary */
int shallow_is_boundary(const hash_t *commit_hash) {
    hash_t *shallow_commits = NULL;
    size_t count = 0;

    if (shallow_read_commits(&shallow_commits, &count) < 0) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        if (hash_equal(&shallow_commits[i], commit_hash)) {
            free(shallow_commits);
            return 1;
        }
    }

    free(shallow_commits);
    return 0;
}

/* Deepen a shallow repository */
int shallow_deepen(int depth) {
    if (!shallow_is_repository_shallow()) {
        fprintf(stderr, "Repository is not shallow\n");
        return -1;
    }

    hash_t *old_shallow = NULL;
    size_t old_count = 0;

    if (shallow_read_commits(&old_shallow, &old_count) < 0) {
        return -1;
    }

    /* Collect new shallow boundary commits */
    hash_t *new_shallow = malloc(old_count * 10 * sizeof(hash_t));
    if (!new_shallow) {
        free(old_shallow);
        return -1;
    }

    size_t new_count = 0;

    /* Walk back 'depth' commits from each old shallow commit */
    for (size_t i = 0; i < old_count; i++) {
        hash_t current = old_shallow[i];

        for (int d = 0; d < depth; d++) {
            commit_t commit;
            if (commit_read(&current, &commit) < 0) break;

            /* Check if it has a parent */
            int has_parent = 0;
            for (int j = 0; j < HASH_SIZE; j++) {
                if (commit.parent.hash[j]) {
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

        /* Add to new shallow boundary */
        new_shallow[new_count++] = current;
    }

    /* Update shallow file */
    int result = shallow_mark(new_shallow, new_count);

    free(old_shallow);
    free(new_shallow);

    return result;
}

/* Collect commits up to a certain depth from HEAD */
int shallow_collect_commits(const hash_t *start, int depth, hash_t **commits_out, size_t *count_out) {
    if (depth <= 0) {
        *commits_out = NULL;
        *count_out = 0;
        return 0;
    }

    size_t capacity = depth + 10;
    hash_t *commits = malloc(capacity * sizeof(hash_t));
    if (!commits) {
        return -1;
    }

    size_t count = 0;
    hash_t current = *start;

    for (int d = 0; d < depth; d++) {
        /* Add current commit */
        if (count >= capacity) {
            capacity *= 2;
            hash_t *new_commits = realloc(commits, capacity * sizeof(hash_t));
            if (!new_commits) {
                free(commits);
                return -1;
            }
            commits = new_commits;
        }

        commits[count++] = current;

        /* Read commit to get parent */
        commit_t commit;
        if (commit_read(&current, &commit) < 0) break;

        /* Check if it has a parent */
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

    *commits_out = commits;
    *count_out = count;
    return 0;
}
