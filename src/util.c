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
