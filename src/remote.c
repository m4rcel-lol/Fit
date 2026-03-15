#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "fit.h"

int remote_add(const char *name, const char *url) {
    /* Check if remote already exists */
    FILE *f = fopen(FIT_CONFIG_FILE, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, name) && strstr(line, "remote")) {
                fclose(f);
                fprintf(stderr, "Remote '%s' already exists\n", name);
                return -1;
            }
        }
        fclose(f);
    }

    /* Append remote to config */
    f = fopen(FIT_CONFIG_FILE, "a");
    if (!f) {
        fprintf(stderr, "Failed to open config file\n");
        return -1;
    }

    fprintf(f, "[remote \"%s\"]\n", name);
    fprintf(f, "    url = %s\n", url);
    fclose(f);

    return 0;
}

int remote_remove(const char *name) {
    FILE *f = fopen(FIT_CONFIG_FILE, "r");
    if (!f) {
        fprintf(stderr, "No remotes configured\n");
        return -1;
    }

    /* Read entire config into memory */
    char *config_data = NULL;
    size_t config_size = 0;
    fseek(f, 0, SEEK_END);
    config_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    config_data = malloc(config_size + 1);
    if (!config_data) {
        fclose(f);
        return -1;
    }

    if (fread(config_data, 1, config_size, f) != config_size) {
        free(config_data);
        fclose(f);
        return -1;
    }
    config_data[config_size] = 0;
    fclose(f);

    /* Create new config without the specified remote */
    char search_str[256];
    snprintf(search_str, sizeof(search_str), "[remote \"%s\"]", name);

    char *remote_start = strstr(config_data, search_str);
    if (!remote_start) {
        free(config_data);
        fprintf(stderr, "Remote '%s' not found\n", name);
        return -1;
    }

    /* Find the end of this remote section (next [ or end of file) */
    char *remote_end = remote_start + strlen(search_str);
    while (*remote_end && *remote_end != '[') {
        remote_end++;
        if (*remote_end == '\n' && *(remote_end + 1) != ' ' && *(remote_end + 1) != '\t') {
            break;
        }
    }

    /* Write new config without the remote */
    f = fopen(FIT_CONFIG_FILE, "w");
    if (!f) {
        free(config_data);
        fprintf(stderr, "Failed to update config\n");
        return -1;
    }

    /* Write everything before the remote */
    fwrite(config_data, 1, remote_start - config_data, f);

    /* Skip the remote section and write everything after */
    if (*remote_end) {
        fputs(remote_end, f);
    }

    fclose(f);
    free(config_data);

    return 0;
}

int remote_list(void) {
    FILE *f = fopen(FIT_CONFIG_FILE, "r");
    if (!f) {
        return 0;  /* No config means no remotes */
    }

    char line[512];
    char current_remote[256] = {0};
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Check for remote section header */
        if (sscanf(line, "[remote \"%255[^\"]", current_remote) == 1) {
            count++;
        }
        /* Check for URL line */
        else if (current_remote[0] && strstr(line, "url =")) {
            char *url = strchr(line, '=');
            if (url) {
                url++;  /* Skip '=' */
                while (*url == ' ' || *url == '\t') url++;  /* Skip whitespace */

                /* Remove trailing newline */
                char *newline = strchr(url, '\n');
                if (newline) *newline = 0;

                printf("%-15s %s\n", current_remote, url);
                current_remote[0] = 0;  /* Reset for next remote */
            }
        }
    }

    fclose(f);
    return count;
}

int remote_get_url(const char *name, char *url, size_t url_size) {
    FILE *f = fopen(FIT_CONFIG_FILE, "r");
    if (!f) {
        return -1;
    }

    char line[512];
    char search_str[256];
    snprintf(search_str, sizeof(search_str), "[remote \"%s\"]", name);
    int found_remote = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, search_str)) {
            found_remote = 1;
            continue;
        }

        if (found_remote && strstr(line, "url =")) {
            char *url_start = strchr(line, '=');
            if (url_start) {
                url_start++;
                while (*url_start == ' ' || *url_start == '\t') url_start++;

                strncpy(url, url_start, url_size - 1);
                url[url_size - 1] = 0;

                /* Remove trailing newline */
                char *newline = strchr(url, '\n');
                if (newline) *newline = 0;

                fclose(f);
                return 0;
            }
        }

        /* If we're in a different section, stop looking */
        if (found_remote && line[0] == '[') {
            break;
        }
    }

    fclose(f);
    return -1;
}
