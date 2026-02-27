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
static void cmd_gc(void);
static void cmd_snapshot(int argc, char **argv);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fit <command> [args]\n");
        return 1;
    }
    
    if (strcmp(argv[1], "init") == 0) cmd_init();
    else if (strcmp(argv[1], "add") == 0) cmd_add(argc - 2, argv + 2);
    else if (strcmp(argv[1], "commit") == 0) cmd_commit(argc - 2, argv + 2);
    else if (strcmp(argv[1], "log") == 0) cmd_log();
    else if (strcmp(argv[1], "status") == 0) cmd_status();
    else if (strcmp(argv[1], "branch") == 0) cmd_branch(argc - 2, argv + 2);
    else if (strcmp(argv[1], "checkout") == 0) cmd_checkout(argc - 2, argv + 2);
    else if (strcmp(argv[1], "daemon") == 0) cmd_daemon(argc - 2, argv + 2);
    else if (strcmp(argv[1], "push") == 0) cmd_push(argc - 2, argv + 2);
    else if (strcmp(argv[1], "gc") == 0) cmd_gc();
    else if (strcmp(argv[1], "snapshot") == 0) cmd_snapshot(argc - 2, argv + 2);
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }
    
    return 0;
}

static void cmd_init(void) {
    mkdirp(FIT_OBJECTS_DIR);
    mkdirp(FIT_HEADS_DIR);
    
    FILE *f = fopen(FIT_HEAD_FILE, "w");
    fprintf(f, "ref: refs/heads/main\n");
    fclose(f);
    
    f = fopen(FIT_INDEX_FILE, "w");
    fclose(f);
    
    printf("Initialized empty Fit repository in %s\n", FIT_DIR);
}

static void cmd_add(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
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
    if (argc < 2 || strcmp(argv[0], "-m") != 0) {
        fprintf(stderr, "Usage: fit commit -m <message>\n");
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
    commit.message = argv[1];
    commit.timestamp = time(NULL);
    
    hash_t commit_hash;
    commit_write(&commit, &commit_hash);
    ref_update_head(&commit_hash);
    
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_hash, hex);
    printf("Created commit %.8s\n", hex);
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
        
        printf("commit %s\n", hex);
        printf("Author: %s\n", commit.author);
        printf("Date: %s", ctime(&commit.timestamp));
        printf("\n    %s\n\n", commit.message);
        
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
    
    index_entry_t *entries;
    index_read(&entries);
    
    if (entries) {
        printf("\nChanges to be committed:\n");
        for (index_entry_t *e = entries; e; e = e->next) {
            printf("  %s\n", e->path);
        }
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
        fprintf(stderr, "Usage: fit checkout <branch>\n");
        return;
    }
    
    char ref_name[256];
    snprintf(ref_name, sizeof(ref_name), "heads/%s", argv[0]);
    
    hash_t hash;
    if (ref_read(ref_name, &hash) < 0) {
        fprintf(stderr, "Branch %s not found\n", argv[0]);
        return;
    }
    
    FILE *f = fopen(FIT_HEAD_FILE, "w");
    fprintf(f, "ref: refs/%s\n", ref_name);
    fclose(f);
    
    printf("Switched to branch %s\n", argv[0]);
}

static void cmd_daemon(int argc, char **argv) {
    int port = 9418;
    
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
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
