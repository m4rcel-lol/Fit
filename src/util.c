#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "fit.h"

int mkdirp(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) < 0 && errno != EEXIST) return -1;
    return 0;
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

char* read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long file_size = ftell(f);
    if (file_size < 0) {
        fclose(f);
        return NULL;
    }
    *size = (size_t)file_size;

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *data = malloc(*size + 1);  // +1 for safety null terminator
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(data, 1, *size, f);
    if (bytes_read != *size) {
        free(data);
        fclose(f);
        return NULL;
    }

    data[*size] = '\0';  // Ensure null termination for text files
    fclose(f);

    return data;
}

int write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    size_t written = fwrite(data, 1, size, f);
    int flush_result = fflush(f);

    if (written != size || flush_result != 0) {
        fclose(f);
        return -1;
    }

    if (fclose(f) != 0) {
        return -1;
    }

    return 0;
}

/* Validate path to prevent directory traversal attacks */
int is_safe_path(const char *path) {
    if (!path || path[0] == '\0') {
        return 0;  // Empty path not allowed
    }

    // Check for absolute paths
    if (path[0] == '/') {
        return 0;  // Absolute paths not allowed
    }

    // Check for directory traversal sequences
    const char *p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.') {
            // Check if it's actually ".." (not just part of filename)
            if ((p == path || p[-1] == '/') && (p[2] == '/' || p[2] == '\0')) {
                return 0;  // ".." traversal not allowed
            }
        }
        p++;
    }

    // Check for overly long paths
    if (strlen(path) > 1024) {
        return 0;
    }

    return 1;  // Path is safe
}

/* Validate tag/branch name to prevent directory traversal */
int is_valid_ref_name(const char *name) {
    if (!name || name[0] == '\0') {
        return 0;  // Empty name not allowed
    }

    // Check for hidden files (starting with .)
    if (name[0] == '.') {
        return 0;  // Names starting with . not allowed
    }

    // Check for path separators
    const char *p = name;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            return 0;  // Path separators not allowed
        }
        p++;
    }

    // Check for reasonable length (max 255 characters)
    if (strlen(name) > 255) {
        return 0;
    }

    return 1;  // Name is valid
}
