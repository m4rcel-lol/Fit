#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "fit.h"

#define PROTOCOL_VERSION 1
#define PROTOCOL_MIN_VERSION 1
#define PROTOCOL_MAX_VERSION 2
#define CMD_SEND_OBJECTS 1
#define CMD_REQUEST_OBJECTS 2
#define CMD_NEGOTIATE 3
#define SOCKET_TIMEOUT_SEC 30

// Capability flags (bitfield)
#define CAP_MULTI_THREADED (1 << 0)
#define CAP_COMPRESSION    (1 << 1)
#define CAP_STREAMING      (1 << 2)

// Protocol negotiation structure
typedef struct {
    uint8_t min_version;
    uint8_t max_version;
    uint32_t capabilities;
} protocol_caps_t;

// Global flag for graceful shutdown
static volatile sig_atomic_t daemon_running = 1;

// Structure to pass client connection info to thread
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_info_t;

// Signal handler for graceful shutdown
static void handle_shutdown(int sig) {
    (void)sig;  // Unused parameter
    daemon_running = 0;
    printf("\nShutting down daemon gracefully...\n");
}

// Helper function for reliable write
static ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t written = 0;
    const char *ptr = (const char *)buf;

    while (written < count) {
        ssize_t n = write(fd, ptr + written, count - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return written;
        written += n;
    }
    return written;
}

// Helper function for setting socket timeouts
static int set_socket_timeout(int sock, int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    return 0;
}

// Get server capabilities
static protocol_caps_t get_server_capabilities(void) {
    protocol_caps_t caps;
    caps.min_version = PROTOCOL_MIN_VERSION;
    caps.max_version = PROTOCOL_MAX_VERSION;
    caps.capabilities = CAP_MULTI_THREADED | CAP_COMPRESSION | CAP_STREAMING;
    return caps;
}

// Negotiate protocol version with client
static int negotiate_protocol(int client_fd, uint8_t *negotiated_version, uint32_t *negotiated_caps) {
    protocol_caps_t server_caps = get_server_capabilities();
    protocol_caps_t client_caps;

    // Receive client capabilities
    if (read(client_fd, &client_caps.min_version, 1) != 1 ||
        read(client_fd, &client_caps.max_version, 1) != 1 ||
        read(client_fd, &client_caps.capabilities, sizeof(uint32_t)) != sizeof(uint32_t)) {
        fprintf(stderr, "Failed to read client capabilities\n");
        return -1;
    }

    client_caps.capabilities = ntohl(client_caps.capabilities);

    // Send server capabilities
    uint32_t caps_network = htonl(server_caps.capabilities);
    if (write_all(client_fd, &server_caps.min_version, 1) != 1 ||
        write_all(client_fd, &server_caps.max_version, 1) != 1 ||
        write_all(client_fd, &caps_network, sizeof(uint32_t)) != sizeof(uint32_t)) {
        fprintf(stderr, "Failed to send server capabilities\n");
        return -1;
    }

    // Negotiate version: use highest common version
    uint8_t max_common = (client_caps.max_version < server_caps.max_version)
                          ? client_caps.max_version : server_caps.max_version;
    uint8_t min_required = (client_caps.min_version > server_caps.min_version)
                            ? client_caps.min_version : server_caps.min_version;

    if (max_common < min_required) {
        fprintf(stderr, "No compatible protocol version\n");
        return -1;
    }

    *negotiated_version = max_common;
    *negotiated_caps = server_caps.capabilities & client_caps.capabilities;

    printf("Negotiated protocol version %d with capabilities 0x%x\n",
           *negotiated_version, *negotiated_caps);

    return 0;
}

// Client-side protocol negotiation
static int client_negotiate_protocol(int sock, uint8_t *negotiated_version, uint32_t *negotiated_caps) {
    protocol_caps_t client_caps;
    client_caps.min_version = PROTOCOL_MIN_VERSION;
    client_caps.max_version = PROTOCOL_MAX_VERSION;
    client_caps.capabilities = CAP_MULTI_THREADED | CAP_COMPRESSION | CAP_STREAMING;

    // Send negotiation command
    uint8_t version = PROTOCOL_MAX_VERSION;
    uint8_t cmd = CMD_NEGOTIATE;
    if (write_all(sock, &version, 1) != 1 || write_all(sock, &cmd, 1) != 1) {
        fprintf(stderr, "Failed to send negotiation header\n");
        return -1;
    }

    // Send client capabilities
    uint32_t caps_network = htonl(client_caps.capabilities);
    if (write_all(sock, &client_caps.min_version, 1) != 1 ||
        write_all(sock, &client_caps.max_version, 1) != 1 ||
        write_all(sock, &caps_network, sizeof(uint32_t)) != sizeof(uint32_t)) {
        fprintf(stderr, "Failed to send client capabilities\n");
        return -1;
    }

    // Receive server capabilities
    protocol_caps_t server_caps;
    if (read(sock, &server_caps.min_version, 1) != 1 ||
        read(sock, &server_caps.max_version, 1) != 1 ||
        read(sock, &server_caps.capabilities, sizeof(uint32_t)) != sizeof(uint32_t)) {
        fprintf(stderr, "Failed to read server capabilities\n");
        return -1;
    }

    server_caps.capabilities = ntohl(server_caps.capabilities);

    // Negotiate version
    uint8_t max_common = (client_caps.max_version < server_caps.max_version)
                          ? client_caps.max_version : server_caps.max_version;
    uint8_t min_required = (client_caps.min_version > server_caps.min_version)
                            ? client_caps.min_version : server_caps.min_version;

    if (max_common < min_required) {
        fprintf(stderr, "No compatible protocol version\n");
        return -1;
    }

    *negotiated_version = max_common;
    *negotiated_caps = client_caps.capabilities & server_caps.capabilities;

    printf("Negotiated protocol version %d with capabilities 0x%x\n",
           *negotiated_version, *negotiated_caps);

    return 0;
}

// Handle client connection (called from thread)
static void handle_client(int client_fd, struct sockaddr_in client_addr) {
    // Set timeout on client socket
    if (set_socket_timeout(client_fd, SOCKET_TIMEOUT_SEC) < 0) {
        fprintf(stderr, "Warning: Failed to set socket timeout\n");
    }

    printf("Client connected from %s\n", inet_ntoa(client_addr.sin_addr));

    uint8_t version, cmd;
    if (read(client_fd, &version, 1) != 1 || read(client_fd, &cmd, 1) != 1) {
        fprintf(stderr, "Failed to read protocol header\n");
        close(client_fd);
        return;
    }

    uint8_t negotiated_version = version;
    uint32_t negotiated_caps = 0;

    // Handle protocol negotiation for version 2+
    if (cmd == CMD_NEGOTIATE) {
        if (negotiate_protocol(client_fd, &negotiated_version, &negotiated_caps) < 0) {
            fprintf(stderr, "Protocol negotiation failed\n");
            close(client_fd);
            return;
        }

        // Read actual command after negotiation
        if (read(client_fd, &cmd, 1) != 1) {
            fprintf(stderr, "Failed to read command after negotiation\n");
            close(client_fd);
            return;
        }
    } else {
        // Legacy protocol (version 1), no negotiation
        if (version != PROTOCOL_VERSION) {
            fprintf(stderr, "Protocol version mismatch: got %d, expected %d\n", version, PROTOCOL_VERSION);
            close(client_fd);
            return;
        }
    }

    if (cmd == CMD_SEND_OBJECTS) {
        printf("Receiving objects...\n");
        char pack_file[256];
        snprintf(pack_file, sizeof(pack_file), "/tmp/fit_recv_%d_%ld.pack", getpid(), (long)pthread_self());

        FILE *f = fopen(pack_file, "wb");
        if (!f) {
            perror("Failed to create pack file");
            close(client_fd);
            return;
        }

        char buf[8192];
        ssize_t n;
        size_t total = 0;
        while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
            size_t written = fwrite(buf, 1, n, f);
            if (written != (size_t)n) {
                fprintf(stderr, "Failed to write to pack file\n");
                fclose(f);
                close(client_fd);
                unlink(pack_file);
                return;
            }
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
                    uint32_t version_val, num_objects;
                    if (fread(&version_val, 4, 1, pf) == 1 && fread(&num_objects, 4, 1, pf) == 1) {
                        num_objects = ntohl(num_objects);

                        if (num_objects > 0) {
                            uint32_t type, size;
                            if (fread(&type, 4, 1, pf) == 1 &&
                                fread(&size, 4, 1, pf) == 1 &&
                                fread(latest_commit.hash, HASH_SIZE, 1, pf) == 1) {

                                if (ntohl(type) == OBJ_COMMIT) {
                                    if (ref_write("heads/main", &latest_commit) == 0) {
                                        printf("Updated main branch\n");
                                    }
                                }
                            }
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
            return;
        }

        char branch[256] = {0};
        if (branch_len >= sizeof(branch)) {
            fprintf(stderr, "Branch name too long: %zu\n", branch_len);
            close(client_fd);
            return;
        }

        if (branch_len > 0 && read(client_fd, branch, branch_len) != (ssize_t)branch_len) {
            fprintf(stderr, "Failed to read branch name\n");
            close(client_fd);
            return;
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
            snprintf(pack_file, sizeof(pack_file), "/tmp/fit_send_%d_%ld.pack", getpid(), (long)pthread_self());
            if (pack_objects(hashes, count, pack_file) == 0) {
                FILE *f = fopen(pack_file, "rb");
                if (f) {
                    char buf[8192];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                        if (write_all(client_fd, buf, n) != (ssize_t)n) {
                            fprintf(stderr, "Failed to send data to client\n");
                            break;
                        }
                    }
                    fclose(f);
                    unlink(pack_file);
                } else {
                    fprintf(stderr, "Failed to open pack file for reading\n");
                }
            } else {
                fprintf(stderr, "Failed to create pack file\n");
            }
        } else {
            fprintf(stderr, "Branch '%s' not found\n", branch);
        }
    }

    close(client_fd);
}

// Thread entry point
static void *client_thread(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    handle_client(info->client_fd, info->client_addr);
    free(info);
    return NULL;
}

int net_daemon_start(int port) {
    // Setup signal handlers for graceful shutdown
    struct sigaction sa;
    sa.sa_handler = handle_shutdown;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("Warning: Failed to set SIGINT handler");
    }
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        perror("Warning: Failed to set SIGTERM handler");
    }

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

    printf("Fit daemon listening on port %d (multi-threaded)\n", port);
    printf("Press Ctrl+C to stop\n");

    while (daemon_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (!daemon_running) break;  // Shutting down
            perror("Failed to accept connection");
            continue;
        }

        // Allocate client info structure
        client_info_t *info = malloc(sizeof(client_info_t));
        if (!info) {
            fprintf(stderr, "Failed to allocate memory for client info\n");
            close(client_fd);
            continue;
        }

        info->client_fd = client_fd;
        info->client_addr = client_addr;

        // Create detached thread to handle client
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&thread, &attr, client_thread, info) != 0) {
            fprintf(stderr, "Failed to create thread for client\n");
            close(client_fd);
            free(info);
        }

        pthread_attr_destroy(&attr);
    }

    printf("Daemon stopped\n");
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

    // Set socket timeout
    if (set_socket_timeout(sock, SOCKET_TIMEOUT_SEC) < 0) {
        fprintf(stderr, "Warning: Failed to set socket timeout\n");
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        perror("Failed to connect to server");
        close(sock);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);

    printf("Connected to %s:%d\n", host, port);

    // Try protocol negotiation (version 2+), fall back to legacy
    uint8_t negotiated_version = PROTOCOL_VERSION;
    uint32_t negotiated_caps = 0;

    if (client_negotiate_protocol(sock, &negotiated_version, &negotiated_caps) == 0) {
        // Negotiation succeeded, send command
        uint8_t cmd = CMD_SEND_OBJECTS;
        if (write_all(sock, &cmd, 1) != 1) {
            perror("Failed to send command");
            close(sock);
            return -1;
        }
    } else {
        // Negotiation failed, fall back to legacy protocol
        printf("Falling back to legacy protocol v1\n");
        uint8_t version = PROTOCOL_VERSION;
        uint8_t cmd = CMD_SEND_OBJECTS;
        if (write_all(sock, &version, 1) != 1 || write_all(sock, &cmd, 1) != 1) {
            perror("Failed to send protocol header");
            close(sock);
            return -1;
        }
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
        if (write_all(sock, buf, n) != (ssize_t)n) {
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

    // Set socket timeout
    if (set_socket_timeout(sock, SOCKET_TIMEOUT_SEC) < 0) {
        fprintf(stderr, "Warning: Failed to set socket timeout\n");
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        perror("Failed to connect to server");
        close(sock);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);

    printf("Connected to %s:%d\n", host, port);

    // Try protocol negotiation (version 2+), fall back to legacy
    uint8_t negotiated_version = PROTOCOL_VERSION;
    uint32_t negotiated_caps = 0;

    if (client_negotiate_protocol(sock, &negotiated_version, &negotiated_caps) == 0) {
        // Negotiation succeeded, send command
        uint8_t cmd = CMD_REQUEST_OBJECTS;
        if (write_all(sock, &cmd, 1) != 1) {
            perror("Failed to send command");
            close(sock);
            return -1;
        }
    } else {
        // Negotiation failed, fall back to legacy protocol
        printf("Falling back to legacy protocol v1\n");
        uint8_t version = PROTOCOL_VERSION;
        uint8_t cmd = CMD_REQUEST_OBJECTS;
        if (write_all(sock, &version, 1) != 1 || write_all(sock, &cmd, 1) != 1) {
            perror("Failed to send protocol header");
            close(sock);
            return -1;
        }
    }

    size_t branch_len = strlen(branch);
    if (write_all(sock, &branch_len, sizeof(branch_len)) != sizeof(branch_len) ||
        write_all(sock, branch, branch_len) != (ssize_t)branch_len) {
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
        size_t written = fwrite(buf, 1, n, f);
        if (written != (size_t)n) {
            fprintf(stderr, "Failed to write to pack file\n");
            fclose(f);
            close(sock);
            unlink(pack_file);
            return -1;
        }
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
