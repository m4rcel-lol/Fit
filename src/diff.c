#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fit.h"

/* Simple line-based diff implementation */
typedef struct line {
    char *content;
    struct line *next;
} line_t;

static line_t* read_lines(const char *data, size_t size) {
    line_t *head = NULL, *tail = NULL;
    const char *start = data;
    const char *end = data + size;

    while (start < end) {
        const char *newline = memchr(start, '\n', end - start);
        size_t len = newline ? (newline - start + 1) : (end - start);

        line_t *line = malloc(sizeof(line_t));
        if (!line) {
            fprintf(stderr, "Failed to allocate memory for line\n");
            return head;
        }
        line->content = malloc(len + 1);
        if (!line->content) {
            fprintf(stderr, "Failed to allocate memory for line content\n");
            free(line);
            return head;
        }
        memcpy(line->content, start, len);
        line->content[len] = '\0';
        line->next = NULL;

        if (tail) {
            tail->next = line;
            tail = line;
        } else {
            head = tail = line;
        }

        start += len;
    }

    return head;
}

static void free_lines(line_t *lines) {
    while (lines) {
        line_t *next = lines->next;
        free(lines->content);
        free(lines);
        lines = next;
    }
}

int diff_blobs(const hash_t *hash1, const hash_t *hash2) {
    object_t obj1 = {0}, obj2 = {0};

    if (object_read(hash1, &obj1) < 0) {
        fprintf(stderr, "Failed to read first object\n");
        return -1;
    }

    if (object_read(hash2, &obj2) < 0) {
        fprintf(stderr, "Failed to read second object\n");
        object_free(&obj1);
        return -1;
    }

    /* Simple diff: show if files are different */
    if (obj1.size != obj2.size || memcmp(obj1.data, obj2.data, obj1.size) != 0) {
        printf("Files differ\n");

        /* Show basic line-by-line comparison */
        line_t *lines1 = read_lines(obj1.data, obj1.size);
        line_t *lines2 = read_lines(obj2.data, obj2.size);

        line_t *l1 = lines1, *l2 = lines2;
        int linenum = 1;

        while (l1 || l2) {
            if (!l2 || (l1 && l2 && strcmp(l1->content, l2->content) != 0)) {
                if (l1) {
                    printf("- %s", l1->content);
                    if (l1->content[strlen(l1->content)-1] != '\n') printf("\n");
                    l1 = l1->next;
                }
            } else if (!l1 || strcmp(l1->content, l2->content) != 0) {
                if (l2) {
                    printf("+ %s", l2->content);
                    if (l2->content[strlen(l2->content)-1] != '\n') printf("\n");
                    l2 = l2->next;
                }
            } else {
                l1 = l1->next;
                l2 = l2->next;
            }
            linenum++;
        }

        free_lines(lines1);
        free_lines(lines2);
    }

    object_free(&obj1);
    object_free(&obj2);
    return 0;
}

int diff_trees(const hash_t *tree1, const hash_t *tree2, const char *prefix) {
    tree_entry_t *entries1 = tree_read(tree1);
    tree_entry_t *entries2 = tree_read(tree2);

    /* Compare entries */
    tree_entry_t *e1 = entries1;
    while (e1) {
        /* Find matching entry in tree2 */
        tree_entry_t *e2 = entries2;
        int found = 0;

        while (e2) {
            if (strcmp(e1->name, e2->name) == 0) {
                found = 1;
                if (!hash_equal(&e1->hash, &e2->hash)) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s%s", prefix, e1->name);

                    if (e1->mode == 040000) {
                        /* Recursively diff directories */
                        char new_prefix[1024];
                        snprintf(new_prefix, sizeof(new_prefix), "%s/", path);
                        diff_trees(&e1->hash, &e2->hash, new_prefix);
                    } else {
                        printf("Modified: %s\n", path);
                        diff_blobs(&e1->hash, &e2->hash);
                    }
                }
                break;
            }
            e2 = e2->next;
        }

        if (!found) {
            printf("Deleted: %s%s\n", prefix, e1->name);
        }

        e1 = e1->next;
    }

    /* Check for new files */
    tree_entry_t *e2 = entries2;
    while (e2) {
        tree_entry_t *e1 = entries1;
        int found = 0;

        while (e1) {
            if (strcmp(e1->name, e2->name) == 0) {
                found = 1;
                break;
            }
            e1 = e1->next;
        }

        if (!found) {
            printf("Added: %s%s\n", prefix, e2->name);
        }

        e2 = e2->next;
    }

    tree_free(entries1);
    tree_free(entries2);
    return 0;
}

int diff_commits(const hash_t *commit1, const hash_t *commit2) {
    commit_t c1 = {0}, c2 = {0};

    if (commit_read(commit1, &c1) < 0) {
        fprintf(stderr, "Failed to read first commit\n");
        return -1;
    }

    if (commit_read(commit2, &c2) < 0) {
        fprintf(stderr, "Failed to read second commit\n");
        commit_free(&c1);
        return -1;
    }

    printf("Comparing commits:\n");
    char hex1[HASH_HEX_SIZE + 1], hex2[HASH_HEX_SIZE + 1];
    hash_to_hex(commit1, hex1);
    hash_to_hex(commit2, hex2);
    printf("  %.8s..%.8s\n\n", hex1, hex2);

    diff_trees(&c1.tree, &c2.tree, "");

    commit_free(&c1);
    commit_free(&c2);
    return 0;
}
