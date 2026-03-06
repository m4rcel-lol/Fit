#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "fit.h"

static void cmd_init(void);
static void cmd_add(int argc, char **argv);
static void cmd_commit(int argc, char **argv);
static void cmd_log(void);
static void cmd_status(void);
static void cmd_branch(int argc, char **argv);
static void cmd_checkout(int argc, char **argv);
static void cmd_daemon(int argc, char **argv);
static void cmd_push(int argc, char **argv);
static void cmd_pull(int argc, char **argv);
static void cmd_clone(int argc, char **argv);
static void cmd_restore(int argc, char **argv);
static void cmd_gc(void);
static void cmd_snapshot(int argc, char **argv);
static void cmd_diff(int argc, char **argv);
static void cmd_tag(int argc, char **argv);
static void cmd_remote(int argc, char **argv);
static void cmd_stash(int argc, char **argv);
static void cmd_merge(int argc, char **argv);
static void cmd_verify(void);
static void cmd_init_signing(void);
static void cmd_verify_commit(int argc, char **argv);
static void cmd_help(void);
static void cmd_credits(void);

int main(int argc, char **argv) {
    if (argc < 2) {
        cmd_help();
        return 1;
    }
    
    if (strcmp(argv[1], "init") == 0) cmd_init();
    else if (strcmp(argv[1], "init-signing") == 0) cmd_init_signing();
    else if (strcmp(argv[1], "add") == 0) cmd_add(argc - 2, argv + 2);
    else if (strcmp(argv[1], "commit") == 0) cmd_commit(argc - 2, argv + 2);
    else if (strcmp(argv[1], "log") == 0) cmd_log();
    else if (strcmp(argv[1], "status") == 0) cmd_status();
    else if (strcmp(argv[1], "branch") == 0) cmd_branch(argc - 2, argv + 2);
    else if (strcmp(argv[1], "checkout") == 0) cmd_checkout(argc - 2, argv + 2);
    else if (strcmp(argv[1], "daemon") == 0) cmd_daemon(argc - 2, argv + 2);
    else if (strcmp(argv[1], "push") == 0) cmd_push(argc - 2, argv + 2);
    else if (strcmp(argv[1], "pull") == 0) cmd_pull(argc - 2, argv + 2);
    else if (strcmp(argv[1], "clone") == 0) cmd_clone(argc - 2, argv + 2);
    else if (strcmp(argv[1], "restore") == 0) cmd_restore(argc - 2, argv + 2);
    else if (strcmp(argv[1], "gc") == 0) cmd_gc();
    else if (strcmp(argv[1], "snapshot") == 0) cmd_snapshot(argc - 2, argv + 2);
    else if (strcmp(argv[1], "diff") == 0) cmd_diff(argc - 2, argv + 2);
    else if (strcmp(argv[1], "tag") == 0) cmd_tag(argc - 2, argv + 2);
    else if (strcmp(argv[1], "remote") == 0) cmd_remote(argc - 2, argv + 2);
    else if (strcmp(argv[1], "stash") == 0) cmd_stash(argc - 2, argv + 2);
    else if (strcmp(argv[1], "merge") == 0) cmd_merge(argc - 2, argv + 2);
    else if (strcmp(argv[1], "verify") == 0) cmd_verify();
    else if (strcmp(argv[1], "verify-commit") == 0) cmd_verify_commit(argc - 2, argv + 2);
    else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) cmd_help();
    else if (strcmp(argv[1], "credits") == 0) cmd_credits();
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        cmd_help();
        return 1;
    }
    
    return 0;
}

static void cmd_init(void) {
    if (mkdirp(FIT_OBJECTS_DIR) != 0) {
        fprintf(stderr, "Error: Failed to create objects directory\n");
        return;
    }
    if (mkdirp(FIT_HEADS_DIR) != 0) {
        fprintf(stderr, "Error: Failed to create refs/heads directory\n");
        return;
    }

    FILE *f = fopen(FIT_HEAD_FILE, "w");
    if (!f) {
        fprintf(stderr, "Error: Failed to create HEAD file\n");
        return;
    }
    fprintf(f, "ref: refs/heads/main\n");
    fclose(f);

    f = fopen(FIT_INDEX_FILE, "w");
    if (!f) {
        fprintf(stderr, "Error: Failed to create index file\n");
        return;
    }
    fclose(f);

    printf("Initialized empty Fit repository in %s\n", FIT_DIR);
}

static void cmd_add(int argc, char **argv) {
    if (argc == 0) {
        fprintf(stderr, "Usage: fit add <file>...\n");
        return;
    }

    for (int i = 0; i < argc; i++) {
        if (!is_safe_path(argv[i])) {
            fprintf(stderr, "Error: Invalid or unsafe file path: %s\n", argv[i]);
            continue;
        }
        if (strlen(argv[i]) > 1024) {
            fprintf(stderr, "Error: File path too long: %s\n", argv[i]);
            continue;
        }
        if (index_add(argv[i]) == 0) {
            printf("Added %s\n", argv[i]);
        } else {
            fprintf(stderr, "Failed to add %s\n", argv[i]);
        }
    }
}

static hash_t build_tree_from_index(void) {
    index_entry_t *entries;
    index_read(&entries);
    
    tree_entry_t *tree_head = NULL, *tree_tail = NULL;

    for (index_entry_t *e = entries; e; e = e->next) {
        tree_entry_t *te = tree_entry_new(e->mode, e->path, &e->hash);
        if (!te) {
            fprintf(stderr, "Error: Failed to allocate tree entry\n");
            tree_free(tree_head);
            index_free(entries);
            hash_t zero_hash = {0};
            return zero_hash;
        }
        if (!tree_head) tree_head = te;
        if (tree_tail) tree_tail->next = te;
        tree_tail = te;
    }
    
    hash_t tree_hash;
    tree_write(tree_head, &tree_hash);
    
    tree_free(tree_head);
    index_free(entries);
    
    return tree_hash;
}

static void cmd_commit(int argc, char **argv) {
    int sign_commit = 0;
    int message_index = -1;

    /* Parse arguments */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--sign") == 0) {
            sign_commit = 1;
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            message_index = i + 1;
        }
    }

    if (message_index == -1) {
        fprintf(stderr, "Usage: fit commit -m <message> [--sign|-S]\n");
        return;
    }

    char *message = argv[message_index];

    if (strlen(message) == 0) {
        fprintf(stderr, "Error: Commit message cannot be empty\n");
        return;
    }

    if (strlen(message) > 8192) {
        fprintf(stderr, "Error: Commit message too long (max 8192 characters)\n");
        return;
    }

    hash_t tree_hash = build_tree_from_index();

    commit_t commit = {0};
    commit.tree = tree_hash;

    hash_t parent_hash;
    if (ref_resolve_head(&parent_hash) == 0) {
        commit.parent = parent_hash;
    }

    char *user = getenv("USER");
    if (!user) user = "unknown";
    commit.author = user;
    commit.message = message;
    commit.timestamp = time(NULL);

    /* Sign commit if requested */
    if (sign_commit) {
        if (!signature_has_key()) {
            fprintf(stderr, "Error: No signing key found. Run 'fit init-signing' first.\n");
            return;
        }

        /* Build commit data to sign (without signature field) */
        char tree_hex[HASH_HEX_SIZE + 1];
        char parent_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&commit.tree, tree_hex);
        hash_to_hex(&commit.parent, parent_hex);

        char *data_to_sign;
        size_t sign_len;
        int has_parent = 0;
        for (int i = 0; i < HASH_SIZE; i++) {
            if (commit.parent.hash[i]) {
                has_parent = 1;
                break;
            }
        }

        if (has_parent) {
            sign_len = asprintf(&data_to_sign, "tree %s\nparent %s\nauthor %s %ld\n\n%s",
                               tree_hex, parent_hex, commit.author, commit.timestamp, commit.message);
        } else {
            sign_len = asprintf(&data_to_sign, "tree %s\nauthor %s %ld\n\n%s",
                               tree_hex, commit.author, commit.timestamp, commit.message);
        }

        if ((int)sign_len < 0 || !data_to_sign) {
            fprintf(stderr, "Error: Failed to allocate memory for signing\n");
            return;
        }

        char *signature_hex = NULL;
        size_t sig_len = 0;

        if (signature_sign(data_to_sign, sign_len, &signature_hex, &sig_len) == 0) {
            commit.signature = signature_hex;
            printf("Commit will be signed\n");
        } else {
            fprintf(stderr, "Warning: Failed to sign commit, proceeding unsigned\n");
        }

        free(data_to_sign);
    }

    hash_t commit_hash;
    commit_write(&commit, &commit_hash);
    ref_update_head(&commit_hash);

    if (commit.signature) {
        free(commit.signature);
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_hash, hex);
    printf("Created commit %.8s%s\n", hex, sign_commit ? " (signed)" : "");
}

static void cmd_log(void) {
    hash_t hash;
    if (ref_resolve_head(&hash) < 0) {
        printf("No commits yet\n");
        return;
    }

    while (1) {
        commit_t commit;
        if (commit_read(&hash, &commit) < 0) break;

        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&hash, hex);

        printf("commit %s", hex);
        if (commit.signature) {
            printf(" (signed)");
        }
        printf("\n");

        printf("Author: %s\n", commit.author);
        printf("Date: %s", ctime(&commit.timestamp));
        printf("\n    %s\n\n", commit.message);

        /* Check if we hit a shallow boundary */
        if (shallow_is_boundary(&hash)) {
            printf("(shallow boundary - history truncated)\n");
            commit_free(&commit);
            break;
        }

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

        hash = commit.parent;
        commit_free(&commit);
    }
}

static void cmd_status(void) {
    char *branch = ref_current_branch();
    if (branch) {
        printf("On branch %s\n", branch);
        free(branch);
    } else {
        printf("HEAD detached\n");
    }

    /* Show HEAD commit info */
    hash_t head_hash;
    if (ref_resolve_head(&head_hash) == 0) {
        commit_t commit;
        if (commit_read(&head_hash, &commit) == 0) {
            char hex[HASH_HEX_SIZE + 1];
            hash_to_hex(&head_hash, hex);
            printf("HEAD at %.8s: %s\n", hex, commit.message);
            commit_free(&commit);
        }
    }

    index_entry_t *entries;
    index_read(&entries);

    if (entries) {
        printf("\nChanges to be committed:\n");
        printf("  (use \"fit commit -m <message>\" to commit)\n\n");
        for (index_entry_t *e = entries; e; e = e->next) {
            char hex[HASH_HEX_SIZE + 1];
            hash_to_hex(&e->hash, hex);
            printf("  \033[32mmodified:\033[0m   %-30s (%.8s)\n", e->path, hex);
        }
    } else {
        printf("\nNo changes staged for commit\n");
        printf("  (use \"fit add <file>\" to stage changes)\n");
    }

    index_free(entries);
}

static void cmd_branch(int argc, char **argv) {
    if (argc == 0) {
        DIR *d = opendir(FIT_HEADS_DIR);
        if (!d) return;
        
        char *current = ref_current_branch();
        struct dirent *entry;
        while ((entry = readdir(d))) {
            if (entry->d_name[0] == '.') continue;
            if (current && strcmp(entry->d_name, current) == 0) {
                printf("* %s\n", entry->d_name);
            } else {
                printf("  %s\n", entry->d_name);
            }
        }
        closedir(d);
        if (current) free(current);
    } else {
        hash_t hash;
        if (ref_resolve_head(&hash) < 0) {
            fprintf(stderr, "No commits yet\n");
            return;
        }
        
        char ref_name[256];
        snprintf(ref_name, sizeof(ref_name), "heads/%s", argv[0]);
        ref_write(ref_name, &hash);
        printf("Created branch %s\n", argv[0]);
    }
}

static void cmd_checkout(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: fit checkout <branch|commit>\n");
        return;
    }
    
    hash_t hash;
    char ref_name[256];
    snprintf(ref_name, sizeof(ref_name), "heads/%s", argv[0]);
    
    if (ref_read(ref_name, &hash) == 0) {
        FILE *f = fopen(FIT_HEAD_FILE, "w");
        if (!f) {
            fprintf(stderr, "Error: Failed to update HEAD file\n");
            return;
        }
        fprintf(f, "ref: refs/%s\n", ref_name);
        fclose(f);
        
        commit_t commit;
        if (commit_read(&hash, &commit) == 0) {
            checkout_tree(&commit.tree, NULL);
            commit_free(&commit);
        }
        
        printf("Switched to branch %s\n", argv[0]);
    } else if (hex_to_hash(argv[0], &hash) == 0) {
        commit_t commit;
        if (commit_read(&hash, &commit) == 0) {
            checkout_tree(&commit.tree, NULL);
            commit_free(&commit);

            FILE *f = fopen(FIT_HEAD_FILE, "w");
            if (!f) {
                fprintf(stderr, "Error: Failed to update HEAD file\n");
                return;
            }
            fprintf(f, "%s\n", argv[0]);
            fclose(f);
            
            printf("Checked out commit %s (detached HEAD)\n", argv[0]);
        } else {
            fprintf(stderr, "Invalid commit hash\n");
        }
    } else {
        fprintf(stderr, "Branch or commit not found: %s\n", argv[0]);
    }
}

static void cmd_daemon(int argc, char **argv) {
    int port = 9418;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Error: Invalid port number. Port must be between 1 and 65535\n");
                return;
            }
            break;
        }
    }

    net_daemon_start(port);
}

static void cmd_push(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fit push <host> <branch>\n");
        return;
    }
    
    char ref_name[256];
    snprintf(ref_name, sizeof(ref_name), "heads/%s", argv[1]);
    
    hash_t hash;
    if (ref_read(ref_name, &hash) < 0) {
        fprintf(stderr, "Branch %s not found\n", argv[1]);
        return;
    }
    
    hash_t hashes[256];
    int count = 0;
    
    hash_t current = hash;
    while (count < 256) {
        hashes[count++] = current;
        
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
    
    if (net_send_objects(argv[0], 9418, hashes, count) == 0) {
        printf("Pushed %d objects to %s\n", count, argv[0]);
    } else {
        fprintf(stderr, "Push failed\n");
    }
}

static void cmd_gc(void) {
    gc_run();
}

static void cmd_snapshot(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[0], "-m") != 0) {
        fprintf(stderr, "Usage: fit snapshot -m <message>\n");
        return;
    }
    
    DIR *d = opendir(".");
    if (!d) return;
    
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (entry->d_name[0] == '.') continue;
        
        struct stat st;
        if (stat(entry->d_name, &st) < 0) continue;
        if (S_ISREG(st.st_mode)) {
            index_add(entry->d_name);
        }
    }
    closedir(d);
    
    char *commit_argv[] = { "-m", argv[1] };
    cmd_commit(2, commit_argv);
    
    printf("Snapshot created\n");
}

static void cmd_pull(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fit pull <host> <branch>\n");
        return;
    }
    
    printf("Pulling from %s...\n", argv[0]);
    
    if (net_recv_objects(argv[0], 9418, argv[1]) == 0) {
        char ref_name[256];
        snprintf(ref_name, sizeof(ref_name), "heads/%s", argv[1]);
        
        hash_t hash;
        if (ref_read(ref_name, &hash) == 0) {
            ref_update_head(&hash);
            printf("Updated branch %s\n", argv[1]);
        }
    } else {
        fprintf(stderr, "Pull failed\n");
    }
}

static void cmd_clone(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fit clone <host> <branch> [directory] [--depth N]\n");
        return;
    }

    const char *host = argv[0];
    const char *branch = argv[1];
    const char *dir = ".";
    int depth = 0; /* 0 means full clone */

    /* Parse optional arguments */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
            depth = atoi(argv[i + 1]);
            if (depth < 1 || depth > 100000) {
                fprintf(stderr, "Error: Invalid depth value (must be between 1 and 100000): %d\n", depth);
                return;
            }
            i++; /* Skip next arg */
        } else if (argv[i][0] != '-') {
            dir = argv[i];
        }
    }

    if (strcmp(dir, ".") != 0) {
        mkdirp(dir);
        if (chdir(dir) < 0) {
            fprintf(stderr, "Failed to enter directory %s\n", dir);
            return;
        }
    }

    cmd_init();

    /* Perform pull (which will respect depth if set) */
    char *pull_argv[] = { (char*)host, (char*)branch };
    cmd_pull(2, pull_argv);

    hash_t hash;
    if (ref_resolve_head(&hash) == 0) {
        /* If shallow clone, mark the repository */
        if (depth > 0) {
            hash_t *commits = NULL;
            size_t count = 0;

            /* Collect commits up to depth */
            if (shallow_collect_commits(&hash, depth, &commits, &count) == 0) {
                /* Mark the last commit as shallow boundary */
                if (count > 0) {
                    hash_t shallow_boundary = commits[count - 1];

                    /* Check if this commit has a parent */
                    commit_t commit;
                    if (commit_read(&shallow_boundary, &commit) == 0) {
                        int has_parent = 0;
                        for (int i = 0; i < HASH_SIZE; i++) {
                            if (commit.parent.hash[i]) {
                                has_parent = 1;
                                break;
                            }
                        }

                        if (has_parent) {
                            /* Mark the parent as shallow */
                            hash_t shallow_commits[1] = { commit.parent };
                            shallow_mark(shallow_commits, 1);
                            printf("Created shallow clone with depth %d\n", depth);
                        }

                        commit_free(&commit);
                    }
                }

                free(commits);
            }
        }

        checkout_commit(&hash);
        printf("Cloned into %s\n", dir);
    }
}

static void cmd_restore(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: fit restore <commit-hash>\n");
        return;
    }
    
    hash_t hash;
    if (hex_to_hash(argv[0], &hash) < 0) {
        fprintf(stderr, "Invalid commit hash\n");
        return;
    }
    
    if (checkout_commit(&hash) == 0) {
        printf("Restored files from commit %s\n", argv[0]);
    } else {
        fprintf(stderr, "Failed to restore commit\n");
    }
}

static void cmd_diff(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fit diff <commit1> <commit2>\n");
        fprintf(stderr, "   or: fit diff <commit>  (compare with HEAD)\n");
        return;
    }

    hash_t hash1, hash2;

    if (argc == 1) {
        /* Compare specified commit with HEAD */
        if (hex_to_hash(argv[0], &hash1) < 0) {
            fprintf(stderr, "Invalid commit hash: %s\n", argv[0]);
            return;
        }
        if (ref_resolve_head(&hash2) < 0) {
            fprintf(stderr, "Failed to resolve HEAD\n");
            return;
        }
    } else {
        /* Compare two commits */
        if (hex_to_hash(argv[0], &hash1) < 0) {
            fprintf(stderr, "Invalid commit hash: %s\n", argv[0]);
            return;
        }
        if (hex_to_hash(argv[1], &hash2) < 0) {
            fprintf(stderr, "Invalid commit hash: %s\n", argv[1]);
            return;
        }
    }

    if (diff_commits(&hash1, &hash2) < 0) {
        fprintf(stderr, "Failed to diff commits\n");
    }
}

static void cmd_tag(int argc, char **argv) {
    if (argc == 0) {
        /* List tags */
        if (tag_list() == 0) {
            printf("No tags found\n");
        }
        return;
    }

    if (strcmp(argv[0], "create") == 0 || strcmp(argv[0], "-a") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: fit tag create <name> [-m <message>]\n");
            return;
        }

        const char *tag_name = argv[1];
        const char *message = NULL;

        /* Check for message */
        if (argc >= 4 && strcmp(argv[2], "-m") == 0) {
            message = argv[3];
        }

        /* Get current HEAD commit */
        hash_t commit_hash;
        if (ref_resolve_head(&commit_hash) < 0) {
            fprintf(stderr, "No commits yet\n");
            return;
        }

        if (tag_create(tag_name, &commit_hash, message) == 0) {
            char hex[HASH_HEX_SIZE + 1];
            hash_to_hex(&commit_hash, hex);
            printf("Created tag '%s' at %.8s\n", tag_name, hex);
        }
    } else if (strcmp(argv[0], "delete") == 0 || strcmp(argv[0], "-d") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: fit tag delete <name>\n");
            return;
        }

        if (tag_delete(argv[1]) == 0) {
            printf("Deleted tag '%s'\n", argv[1]);
        }
    } else {
        /* Assume it's a tag name to create */
        const char *tag_name = argv[0];
        hash_t commit_hash;

        if (ref_resolve_head(&commit_hash) < 0) {
            fprintf(stderr, "No commits yet\n");
            return;
        }

        if (tag_create(tag_name, &commit_hash, NULL) == 0) {
            char hex[HASH_HEX_SIZE + 1];
            hash_to_hex(&commit_hash, hex);
            printf("Created tag '%s' at %.8s\n", tag_name, hex);
        }
    }
}

static void cmd_remote(int argc, char **argv) {
    if (argc == 0) {
        /* List remotes */
        if (remote_list() == 0) {
            printf("No remotes configured\n");
        }
        return;
    }

    if (strcmp(argv[0], "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: fit remote add <name> <url>\n");
            return;
        }

        if (remote_add(argv[1], argv[2]) == 0) {
            printf("Added remote '%s' -> %s\n", argv[1], argv[2]);
        }
    } else if (strcmp(argv[0], "remove") == 0 || strcmp(argv[0], "rm") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: fit remote remove <name>\n");
            return;
        }

        if (remote_remove(argv[1]) == 0) {
            printf("Removed remote '%s'\n", argv[1]);
        }
    } else if (strcmp(argv[0], "list") == 0 || strcmp(argv[0], "-v") == 0) {
        if (remote_list() == 0) {
            printf("No remotes configured\n");
        }
    } else {
        fprintf(stderr, "Unknown remote command: %s\n", argv[0]);
        fprintf(stderr, "Usage: fit remote [add|remove|list]\n");
    }
}

static void cmd_stash(int argc, char **argv) {
    if (argc == 0) {
        /* Save stash with default message */
        stash_save("WIP: uncommitted changes");
        return;
    }

    if (strcmp(argv[0], "save") == 0) {
        const char *message = "WIP: uncommitted changes";
        if (argc >= 3 && strcmp(argv[1], "-m") == 0) {
            message = argv[2];
        }
        stash_save(message);
    } else if (strcmp(argv[0], "list") == 0) {
        stash_list();
    } else if (strcmp(argv[0], "pop") == 0) {
        if (argc >= 2) {
            stash_pop(argv[1]);
        } else {
            stash_pop(NULL);
        }
    } else if (strcmp(argv[0], "drop") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: fit stash drop <stash-name>\n");
            return;
        }
        stash_drop(argv[1]);
    } else {
        fprintf(stderr, "Unknown stash command: %s\n", argv[0]);
        fprintf(stderr, "Usage: fit stash [save|list|pop|drop]\n");
    }
}

static void cmd_merge(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: fit merge <branch>\n");
        return;
    }

    const char *branch_name = argv[0];

    /* Get current branch */
    char *current_branch = ref_current_branch();
    if (!current_branch) {
        fprintf(stderr, "Not currently on a branch\n");
        return;
    }

    /* Get current HEAD */
    hash_t current_hash;
    if (ref_resolve_head(&current_hash) < 0) {
        fprintf(stderr, "Failed to resolve HEAD\n");
        free(current_branch);
        return;
    }

    /* Get target branch hash */
    char ref_name[256];
    snprintf(ref_name, sizeof(ref_name), "heads/%s", branch_name);

    hash_t target_hash;
    if (ref_read(ref_name, &target_hash) < 0) {
        fprintf(stderr, "Branch '%s' not found\n", branch_name);
        free(current_branch);
        return;
    }

    /* Check if already up to date */
    if (hash_equal(&current_hash, &target_hash)) {
        printf("Already up to date.\n");
        free(current_branch);
        return;
    }

    /* Perform three-way merge */
    int merge_result = merge_three_way(&current_hash, &target_hash, current_branch, branch_name);

    if (merge_result == 1) {
        /* Fast-forward merge */
        commit_t target_commit;
        if (commit_read(&target_hash, &target_commit) < 0) {
            fprintf(stderr, "Failed to read target commit\n");
            free(current_branch);
            return;
        }

        if (checkout_tree(&target_commit.tree, NULL) < 0) {
            fprintf(stderr, "Failed to checkout tree\n");
            commit_free(&target_commit);
            free(current_branch);
            return;
        }

        ref_update_head(&target_hash);

        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&target_hash, hex);
        printf("Fast-forward merge: %s -> %.8s\n", branch_name, hex);
        printf("Merged '%s' into '%s'\n", branch_name, current_branch);

        commit_free(&target_commit);
    } else if (merge_result == 0) {
        /* Successful three-way merge - create merge commit */
        printf("Creating merge commit...\n");

        /* Build tree from current working directory */
        index_entry_t *entries = NULL;
        if (index_read(&entries) < 0) {
            fprintf(stderr, "Failed to read index\n");
            free(current_branch);
            return;
        }

        /* Add all modified files to index */
        // In a real implementation, we'd scan the working directory
        // For now, we assume the merge has updated files in place

        hash_t tree_hash;
        tree_entry_t *tree_entries = NULL;

        /* Build tree from index entries */
        for (index_entry_t *e = entries; e; e = e->next) {
            tree_entry_t *new_entry = tree_entry_new(e->mode, e->path, &e->hash);
            if (!new_entry) {
                fprintf(stderr, "Error: Failed to allocate tree entry\n");
                tree_free(tree_entries);
                index_free(entries);
                free(current_branch);
                return;
            }
            new_entry->next = tree_entries;
            tree_entries = new_entry;
        }

        if (tree_write(tree_entries, &tree_hash) < 0) {
            fprintf(stderr, "Failed to write tree\n");
            tree_free(tree_entries);
            index_free(entries);
            free(current_branch);
            return;
        }

        tree_free(tree_entries);
        index_free(entries);

        /* Create merge commit with two parents */
        commit_t merge_commit;
        merge_commit.tree = tree_hash;
        merge_commit.parent = current_hash;  // First parent is current HEAD
        merge_commit.author = getenv("USER") ? getenv("USER") : "unknown";

        char msg[512];
        snprintf(msg, sizeof(msg), "Merge branch '%s' into %s", branch_name, current_branch);
        merge_commit.message = msg;
        merge_commit.timestamp = time(NULL);

        hash_t commit_hash;
        if (commit_write(&merge_commit, &commit_hash) < 0) {
            fprintf(stderr, "Failed to write merge commit\n");
            free(current_branch);
            return;
        }

        /* Update HEAD */
        ref_update_head(&commit_hash);

        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&commit_hash, hex);
        printf("Merge commit created: %.8s\n", hex);
        printf("Merged '%s' into '%s'\n", branch_name, current_branch);
    } else if (merge_result == -2) {
        /* Conflicts detected */
        printf("\nPlease resolve conflicts and commit the result with:\n");
        printf("  fit add <conflicted-files>\n");
        printf("  fit commit -m \"Merge branch '%s' into %s\"\n", branch_name, current_branch);
    } else {
        /* Merge failed */
        fprintf(stderr, "Merge failed\n");
    }

    free(current_branch);
}

static void cmd_verify(void) {
    verify_repository();
}

static void cmd_init_signing(void) {
    if (signature_has_key()) {
        printf("Signing key already exists at .fit/private_key.pem\n");
        printf("Delete it first if you want to generate a new key.\n");
        return;
    }

    printf("Generating RSA-2048 key pair for commit signing...\n");
    if (signature_generate_keypair() == 0) {
        printf("\nYou can now sign commits with: fit commit -m \"message\" --sign\n");
    } else {
        fprintf(stderr, "Failed to generate signing key\n");
    }
}

static void cmd_verify_commit(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: fit verify-commit <commit-hash>\n");
        return;
    }

    hash_t commit_hash;
    if (hex_to_hash(argv[0], &commit_hash) < 0) {
        fprintf(stderr, "Invalid commit hash\n");
        return;
    }

    commit_t commit;
    if (commit_read(&commit_hash, &commit) < 0) {
        fprintf(stderr, "Failed to read commit\n");
        return;
    }

    if (!commit.signature) {
        printf("Commit is not signed\n");
        commit_free(&commit);
        return;
    }

    /* Rebuild commit data without signature for verification */
    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit.tree, tree_hex);
    hash_to_hex(&commit.parent, parent_hex);

    char *data_to_verify;
    size_t data_len;
    int has_parent = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        if (commit.parent.hash[i]) {
            has_parent = 1;
            break;
        }
    }

    if (has_parent) {
        data_len = asprintf(&data_to_verify, "tree %s\nparent %s\nauthor %s %ld\n\n%s",
                           tree_hex, parent_hex, commit.author, commit.timestamp, commit.message);
    } else {
        data_len = asprintf(&data_to_verify, "tree %s\nauthor %s %ld\n\n%s",
                           tree_hex, commit.author, commit.timestamp, commit.message);
    }

    if ((int)data_len < 0 || !data_to_verify) {
        fprintf(stderr, "Error: Failed to allocate memory for verification\n");
        commit_free(&commit);
        return;
    }

    int result = signature_verify(data_to_verify, data_len, commit.signature, strlen(commit.signature));

    free(data_to_verify);

    if (result == 0) {
        printf("✓ Good signature\n");
        printf("  Commit: %.8s\n", argv[0]);
        printf("  Author: %s\n", commit.author);
        commit_free(&commit);
    } else {
        printf("✗ BAD signature\n");
        printf("  Commit: %.8s\n", argv[0]);
        printf("  WARNING: Signature verification failed!\n");
        commit_free(&commit);
    }
}

static void cmd_help(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    FIT - Filesystem Inside Terminal          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("USAGE:\n");
    printf("  fit <command> [arguments]\n\n");
    
    printf("COMMANDS:\n");
    printf("  init                      Initialize a new repository\n");
    printf("  init-signing              Generate RSA key pair for signing commits\n");
    printf("  add <files>               Stage files for commit\n");
    printf("  commit -m <message> [-S]  Create a commit (optionally signed)\n");
    printf("  log                       Show commit history\n");
    printf("  status                    Show repository status\n");
    printf("  diff <commit1> <commit2>  Show differences between commits\n");
    printf("  branch [name]             List or create branches\n");
    printf("  checkout <branch>         Switch to a branch and restore files\n");
    printf("  merge <branch>            Merge a branch into current branch\n");
    printf("  tag [name|-a|-d]          List, create, or delete tags\n");
    printf("  remote [add|rm|list]      Manage remote repositories\n");
    printf("  stash [save|pop|list]     Stash and restore changes\n");
    printf("  snapshot -m <message>     Quick backup of all files\n");
    printf("  push <host> <branch>      Push to remote server\n");
    printf("  pull <host> <branch>      Pull from remote server\n");
    printf("  clone <host> <branch> [dir] [--depth N]  Clone repository (optionally shallow)\n");
    printf("  restore <commit>          Restore files from commit\n");
    printf("  daemon --port <port>      Start server daemon\n");
    printf("  gc                        Run garbage collection\n");
    printf("  verify                    Verify repository integrity\n");
    printf("  verify-commit <hash>      Verify commit signature\n");
    printf("  help                      Show this help message\n");
    printf("  credits                   Show credits and info\n\n");
    
    printf("EXAMPLES:\n");
    printf("  fit init\n");
    printf("  fit snapshot -m \"Daily backup\"\n");
    printf("  fit push 192.168.1.50 main\n");
    printf("  fit pull 192.168.1.50 main\n");
    printf("  fit clone 192.168.1.50 main ~/backup\n");
    printf("  fit restore abc123def456\n");
    printf("  fit daemon --port 9418\n\n");
    
    printf("For more info, see README.md or run 'fit credits'\n");
}

static void cmd_credits(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                              ║\n");
    printf("║                    FIT - Filesystem Inside Terminal          ║\n");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("  A distributed version control and backup system\n");
    printf("  inspired by Git's internal architecture.\n\n");
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    printf("  CREATED BY:     m4rcel-lol\n");
    printf("  WRITTEN IN:     C (C17 standard)\n");
    printf("  INSPIRED BY:    Git by Linus Torvalds\n");
    printf("  LICENSE:        MIT License\n\n");
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    printf("  FEATURES:\n");
    printf("    • Content-addressable storage (SHA-256)\n");
    printf("    • Object model: blob, tree, commit\n");
    printf("    • zlib compression\n");
    printf("    • Custom TCP network protocol\n");
    printf("    • Distributed backup between machines\n");
    printf("    • Branch management\n");
    printf("    • Garbage collection\n\n");
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    printf("  QUICK START:\n");
    printf("    fit init                    # Initialize repository\n");
    printf("    fit snapshot -m \"Backup\"    # Create backup\n");
    printf("    fit push <host> main        # Push to server\n\n");
    
    printf("  SERVER:\n");
    printf("    fit daemon --port 9418      # Start backup server\n\n");
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    printf("  Built with ❤️  in C, designed for backups.\n");
    printf("  Because your filesystem belongs inside your terminal.\n\n");
}
