#ifndef FIT_H
#define FIT_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define FIT_DIR ".fit"
#define FIT_OBJECTS_DIR ".fit/objects"
#define FIT_REFS_DIR ".fit/refs"
#define FIT_HEADS_DIR ".fit/refs/heads"
#define FIT_INDEX_FILE ".fit/index"
#define FIT_HEAD_FILE ".fit/HEAD"
#define FIT_CONFIG_FILE ".fit/config"

#define HASH_SIZE 32
#define HASH_HEX_SIZE 64

typedef enum {
    OBJ_BLOB = 1,
    OBJ_TREE = 2,
    OBJ_COMMIT = 3
} obj_type;

typedef struct {
    uint8_t hash[HASH_SIZE];
} hash_t;

typedef struct {
    char *data;
    size_t size;
    obj_type type;
} object_t;

typedef struct tree_entry {
    uint32_t mode;
    char *name;
    hash_t hash;
    struct tree_entry *next;
} tree_entry_t;

typedef struct {
    hash_t tree;
    hash_t parent;
    char *author;
    char *message;
    char *signature;  /* Hex-encoded signature (NULL if unsigned) */
    time_t timestamp;
} commit_t;

typedef struct index_entry {
    char *path;
    hash_t hash;
    uint32_t mode;
    struct index_entry *next;
} index_entry_t;

/* hash.c */
void hash_data(const void *data, size_t len, hash_t *out);
void hash_to_hex(const hash_t *hash, char *hex);
int hex_to_hash(const char *hex, hash_t *hash);
int hash_equal(const hash_t *a, const hash_t *b);

/* object.c */
int object_write(const object_t *obj, hash_t *out);
int object_read(const hash_t *hash, object_t *obj);
void object_free(object_t *obj);
char* object_path(const hash_t *hash);

/* tree.c */
int tree_write(tree_entry_t *entries, hash_t *out);
tree_entry_t* tree_read(const hash_t *hash);
void tree_free(tree_entry_t *entries);
tree_entry_t* tree_entry_new(uint32_t mode, const char *name, const hash_t *hash);

/* commit.c */
int commit_write(const commit_t *commit, hash_t *out);
int commit_read(const hash_t *hash, commit_t *commit);
void commit_free(commit_t *commit);

/* index.c */
int index_read(index_entry_t **entries);
int index_write(index_entry_t *entries);
int index_add(const char *path);
void index_free(index_entry_t *entries);

/* refs.c */
int ref_read(const char *name, hash_t *hash);
int ref_write(const char *name, const hash_t *hash);
int ref_resolve_head(hash_t *hash);
int ref_update_head(const hash_t *hash);
char* ref_current_branch(void);

/* pack.c */
int pack_objects(const hash_t *hashes, size_t count, const char *pack_file);
int unpack_objects(const char *pack_file);

/* network.c */
int net_daemon_start(int port);
int net_send_objects(const char *host, int port, const hash_t *hashes, size_t count);
int net_recv_objects(const char *host, int port, const char *branch);

/* gc.c */
int gc_run(void);

/* util.c */
int mkdirp(const char *path);
int file_exists(const char *path);
char* read_file(const char *path, size_t *size);
int write_file(const char *path, const void *data, size_t size);
int is_safe_path(const char *path);

/* checkout.c */
int checkout_commit(const hash_t *commit_hash);
int checkout_tree(const hash_t *tree_hash, const char *prefix);

/* diff.c */
int diff_blobs(const hash_t *hash1, const hash_t *hash2);
int diff_trees(const hash_t *tree1, const hash_t *tree2, const char *prefix);
int diff_commits(const hash_t *commit1, const hash_t *commit2);

/* tag.c */
int tag_create(const char *name, const hash_t *commit_hash, const char *message);
int tag_delete(const char *name);
int tag_list(void);
int tag_resolve(const char *name, hash_t *out);

/* remote.c */
int remote_add(const char *name, const char *url);
int remote_remove(const char *name);
int remote_list(void);
int remote_get_url(const char *name, char *url, size_t url_size);

/* stash.c */
int stash_save(const char *message);
int stash_list(void);
int stash_pop(const char *stash_name);
int stash_drop(const char *stash_name);

/* verify.c */
int verify_repository(void);

/* merge.c */
int merge_three_way(const hash_t *current_commit, const hash_t *target_commit,
                    const char *current_branch, const char *target_branch);
int hash_is_null(const hash_t *hash);

/* signature.c */
int signature_generate_keypair(void);
int signature_sign(const char *data, size_t data_len, char **signature_out, size_t *sig_len_out);
int signature_verify(const char *data, size_t data_len, const char *hex_signature, size_t sig_hex_len);
int signature_has_key(void);

/* shallow.c */
int shallow_mark(const hash_t *shallow_commits, size_t count);
int shallow_is_repository_shallow(void);
int shallow_read_commits(hash_t **commits_out, size_t *count_out);
int shallow_is_boundary(const hash_t *commit_hash);
int shallow_deepen(int depth);
int shallow_collect_commits(const hash_t *start, int depth, hash_t **commits_out, size_t *count_out);

#endif
