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
    if (server_fd < 0) return -1;
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 5) < 0) {
        close(server_fd);
        return -1;
    }
    
    printf("Fit daemon listening on port %d\n", port);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) continue;
        
        uint8_t version, cmd;
        read(client_fd, &version, 1);
        read(client_fd, &cmd, 1);
        
        if (version != PROTOCOL_VERSION) {
            close(client_fd);
            continue;
        }
        
        if (cmd == CMD_SEND_OBJECTS) {
            char pack_file[256];
            snprintf(pack_file, sizeof(pack_file), "/tmp/fit_recv_%d.pack", getpid());
            
            FILE *f = fopen(pack_file, "wb");
            char buf[8192];
            ssize_t n;
            while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);
            
            unpack_objects(pack_file);
            unlink(pack_file);
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
    
    if (getaddrinfo(host, port_str, &hints, &result) != 0) return -1;
    
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(result);
        return -1;
    }
    
    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(result);
        return -1;
    }
    
    freeaddrinfo(result);
    
    uint8_t version = PROTOCOL_VERSION;
    uint8_t cmd = CMD_SEND_OBJECTS;
    write(sock, &version, 1);
    write(sock, &cmd, 1);
    
    char pack_file[256];
    snprintf(pack_file, sizeof(pack_file), "/tmp/fit_send_%d.pack", getpid());
    pack_objects(hashes, count, pack_file);
    
    FILE *f = fopen(pack_file, "rb");
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        write(sock, buf, n);
    }
    fclose(f);
    unlink(pack_file);
    
    close(sock);
    return 0;
}

int net_recv_objects(const char *host, int port, const hash_t *hashes, size_t count) {
    (void)host;
    (void)port;
    (void)hashes;
    (void)count;
    return 0;
}
