#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "fit.h"

#define PROTOCOL_VERSION 1
#define CMD_SEND_OBJECTS 1
#define CMD_REQUEST_OBJECTS 2

int net_daemon_start(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create socket");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind to port");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Failed to listen");
        close(server_fd);
        return -1;
    }

    printf("Fit daemon listening on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            perror("Failed to accept connection");
            continue;
        }

        printf("Client connected from %s\n", inet_ntoa(client_addr.sin_addr));

        uint8_t version, cmd;
        if (read(client_fd, &version, 1) != 1 || read(client_fd, &cmd, 1) != 1) {
            fprintf(stderr, "Failed to read protocol header\n");
            close(client_fd);
            continue;
        }

        if (version != PROTOCOL_VERSION) {
            fprintf(stderr, "Protocol version mismatch: got %d, expected %d\n", version, PROTOCOL_VERSION);
            close(client_fd);
            continue;
        }
        
        if (cmd == CMD_SEND_OBJECTS) {
            printf("Receiving objects...\n");
            char pack_file[256];
            snprintf(pack_file, sizeof(pack_file), "/tmp/fit_recv_%d.pack", getpid());

            FILE *f = fopen(pack_file, "wb");
            if (!f) {
                perror("Failed to create pack file");
                close(client_fd);
                continue;
            }

            char buf[8192];
            ssize_t n;
            size_t total = 0;
            while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
                total += n;
            }
            fclose(f);

            printf("Received %zu bytes\n", total);

            if (unpack_objects(pack_file) == 0) {
                printf("Successfully unpacked objects\n");
                hash_t latest_commit = {0};
                FILE *pf = fopen(pack_file, "rb");
                if (pf) {
                    char sig[4];
                    if (fread(sig, 1, 4, pf) == 4) {
                        uint32_t version, num_objects;
                        fread(&version, 4, 1, pf);
                        fread(&num_objects, 4, 1, pf);

                        if (num_objects > 0) {
                            uint32_t type, size;
                            fread(&type, 4, 1, pf);
                            fread(&size, 4, 1, pf);
                            fread(latest_commit.hash, HASH_SIZE, 1, pf);

                            if (ntohl(type) == OBJ_COMMIT) {
                                ref_write("heads/main", &latest_commit);
                                printf("Updated main branch\n");
                            }
                        }
                    }
                    fclose(pf);
                }
            } else {
                fprintf(stderr, "Failed to unpack objects\n");
            }
            unlink(pack_file);
        } else if (cmd == CMD_REQUEST_OBJECTS) {
            printf("Sending objects...\n");
            size_t branch_len;
            if (read(client_fd, &branch_len, sizeof(branch_len)) != sizeof(branch_len)) {
                fprintf(stderr, "Failed to read branch length\n");
                close(client_fd);
                continue;
            }

            char branch[256] = {0};
            if (branch_len < sizeof(branch)) {
                if (read(client_fd, branch, branch_len) != (ssize_t)branch_len) {
                    fprintf(stderr, "Failed to read branch name\n");
                    close(client_fd);
                    continue;
                }
            }
            
            char ref_name[256];
            snprintf(ref_name, sizeof(ref_name), "heads/%s", branch);
            
            hash_t hash;
            if (ref_read(ref_name, &hash) == 0) {
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
                
                char pack_file[256];
                snprintf(pack_file, sizeof(pack_file), "/tmp/fit_send_%d.pack", getpid());
                pack_objects(hashes, count, pack_file);
                
                FILE *f = fopen(pack_file, "rb");
                if (f) {
                    char buf[8192];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                        write(client_fd, buf, n);
                    }
                    fclose(f);
                    unlink(pack_file);
                }
            }
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}

int net_send_objects(const char *host, int port, const hash_t *hashes, size_t count) {
    struct addrinfo hints = {0}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        fprintf(stderr, "Failed to resolve host '%s'\n", host);
        return -1;
    }

    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        perror("Failed to create socket");
        freeaddrinfo(result);
        return -1;
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        perror("Failed to connect to server");
        close(sock);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);

    printf("Connected to %s:%d\n", host, port);

    uint8_t version = PROTOCOL_VERSION;
    uint8_t cmd = CMD_SEND_OBJECTS;
    if (write(sock, &version, 1) != 1 || write(sock, &cmd, 1) != 1) {
        perror("Failed to send protocol header");
        close(sock);
        return -1;
    }

    char pack_file[256];
    snprintf(pack_file, sizeof(pack_file), "/tmp/fit_send_%d.pack", getpid());

    if (pack_objects(hashes, count, pack_file) < 0) {
        fprintf(stderr, "Failed to create pack file\n");
        close(sock);
        return -1;
    }

    FILE *f = fopen(pack_file, "rb");
    if (!f) {
        perror("Failed to open pack file");
        close(sock);
        unlink(pack_file);
        return -1;
    }

    char buf[8192];
    size_t n, total = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (write(sock, buf, n) != (ssize_t)n) {
            perror("Failed to send data");
            fclose(f);
            close(sock);
            unlink(pack_file);
            return -1;
        }
        total += n;
    }
    fclose(f);
    unlink(pack_file);

    printf("Sent %zu bytes\n", total);

    close(sock);
    return 0;
}

int net_recv_objects(const char *host, int port, const char *branch) {
    struct addrinfo hints = {0}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        fprintf(stderr, "Failed to resolve host '%s'\n", host);
        return -1;
    }

    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        perror("Failed to create socket");
        freeaddrinfo(result);
        return -1;
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        perror("Failed to connect to server");
        close(sock);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);

    printf("Connected to %s:%d\n", host, port);

    uint8_t version = PROTOCOL_VERSION;
    uint8_t cmd = CMD_REQUEST_OBJECTS;
    if (write(sock, &version, 1) != 1 || write(sock, &cmd, 1) != 1) {
        perror("Failed to send protocol header");
        close(sock);
        return -1;
    }

    size_t branch_len = strlen(branch);
    if (write(sock, &branch_len, sizeof(branch_len)) != sizeof(branch_len) ||
        write(sock, branch, branch_len) != (ssize_t)branch_len) {
        perror("Failed to send branch name");
        close(sock);
        return -1;
    }

    char pack_file[256];
    snprintf(pack_file, sizeof(pack_file), "/tmp/fit_recv_%d.pack", getpid());

    FILE *f = fopen(pack_file, "wb");
    if (!f) {
        perror("Failed to create pack file");
        close(sock);
        return -1;
    }

    char buf[8192];
    ssize_t n;
    size_t total = 0;
    while ((n = read(sock, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, n, f);
        total += n;
    }
    fclose(f);
    close(sock);

    printf("Received %zu bytes\n", total);

    int ret = unpack_objects(pack_file);
    if (ret < 0) {
        fprintf(stderr, "Failed to unpack objects\n");
    } else {
        printf("Successfully unpacked objects\n");
    }

    unlink(pack_file);
    return ret;
}
