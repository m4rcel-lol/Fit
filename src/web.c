#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../include/fit.h"

#define PORT 8080
#define BUFFER_SIZE 8192
#define SESSION_TIMEOUT 3600

static sqlite3 *db = NULL;

void init_db() {
    sqlite3_open("fitweb.db", &db);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, username TEXT, expires INTEGER)", NULL, NULL, NULL);
}

char* generate_token() {
    static char token[33];
    srand(time(NULL) + rand());
    for (int i = 0; i < 32; i++) {
        token[i] = "0123456789abcdef"[rand() % 16];
    }
    token[32] = '\0';
    return token;
}

int verify_session(const char *token) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT expires FROM sessions WHERE token = ?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    int valid = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        time_t expires = sqlite3_column_int64(stmt, 0);
        valid = (time(NULL) < expires);
    }
    sqlite3_finalize(stmt);
    return valid;
}

void create_session(const char *token) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO sessions VALUES (?, 'm5rcel', ?)", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, time(NULL) + SESSION_TIMEOUT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void send_response(int client, const char *status, const char *content_type, const char *body, const char *cookie) {
    char header[1024];
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "%s"
        "\r\n",
        status, content_type, strlen(body),
        cookie ? cookie : "");
    write(client, header, len);
    write(client, body, strlen(body));
}

void send_login_page(int client) {
    const char *html = 
        "<!DOCTYPE html><html><head><title>Fit Login</title>"
        "<style>body{font-family:monospace;max-width:400px;margin:100px auto;background:#1e1e1e;color:#d4d4d4}"
        "input{display:block;width:100%;margin:10px 0;padding:8px;background:#2d2d2d;border:1px solid #444;color:#d4d4d4}"
        "button{width:100%;padding:10px;background:#0e639c;color:#fff;border:none;cursor:pointer}"
        "button:hover{background:#1177bb}</style></head><body>"
        "<h2>Fit Web Interface</h2>"
        "<form method='POST' action='/login'>"
        "<input name='username' placeholder='Username' required>"
        "<input name='password' type='password' placeholder='Password' required>"
        "<button type='submit'>Login</button></form></body></html>";
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_main_page(int client, const char *token) {
    char html[BUFFER_SIZE];
    char commits[4096] = "";
    
    FILE *fp = popen("cd /home/m5rcel/Fit && ./bin/fit log 2>/dev/null", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            strcat(commits, line);
            strcat(commits, "<br>");
        }
        pclose(fp);
    }
    
    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Fit Repository</title>"
        "<style>body{font-family:monospace;max-width:1200px;margin:20px auto;background:#1e1e1e;color:#d4d4d4}"
        "a{color:#4fc3f7;text-decoration:none}a:hover{text-decoration:underline}"
        ".header{background:#2d2d2d;padding:15px;margin-bottom:20px;border-radius:5px}"
        ".section{background:#2d2d2d;padding:15px;margin:10px 0;border-radius:5px}"
        ".commit{border-left:3px solid #0e639c;padding-left:10px;margin:10px 0}"
        "button{padding:8px 15px;background:#0e639c;color:#fff;border:none;cursor:pointer;margin:5px}"
        "button:hover{background:#1177bb}</style></head><body>"
        "<div class='header'><h1>Fit Repository</h1>"
        "<a href='/logout'>Logout</a> | <a href='/files'>Browse Files</a> | <a href='/download'>Download Archive</a></div>"
        "<div class='section'><h2>Recent Commits</h2><div class='commit'>%s</div></div>"
        "</body></html>",
        commits[0] ? commits : "No commits yet");
    
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_files_page(int client, const char *path) {
    char html[BUFFER_SIZE];
    char files[4096] = "<ul>";
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "/home/m5rcel/Fit%s", path);
    
    DIR *dir = opendir(fullpath);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (strstr(ent->d_name, ".fit")) continue;
            
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", fullpath, ent->d_name);
            struct stat st;
            stat(filepath, &st);
            
            if (S_ISDIR(st.st_mode)) {
                char link[256];
                snprintf(link, sizeof(link), "<li>📁 <a href='/files%s/%s'>%s/</a></li>", path, ent->d_name, ent->d_name);
                strcat(files, link);
            } else {
                char link[256];
                snprintf(link, sizeof(link), "<li>📄 <a href='/raw%s/%s'>%s</a> (%ld bytes)</li>", path, ent->d_name, ent->d_name, st.st_size);
                strcat(files, link);
            }
        }
        closedir(dir);
    }
    strcat(files, "</ul>");
    
    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Browse Files</title>"
        "<style>body{font-family:monospace;max-width:1200px;margin:20px auto;background:#1e1e1e;color:#d4d4d4}"
        "a{color:#4fc3f7;text-decoration:none}a:hover{text-decoration:underline}"
        ".header{background:#2d2d2d;padding:15px;margin-bottom:20px;border-radius:5px}"
        "ul{list-style:none;padding:0}li{padding:8px;background:#2d2d2d;margin:5px 0;border-radius:3px}</style></head><body>"
        "<div class='header'><h1>Browse: %s</h1><a href='/'>← Back to Repository</a></div>"
        "%s</body></html>",
        path, files);
    
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_raw_file(int client, const char *path) {
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "/home/m5rcel/Fit%s", path);
    
    FILE *f = fopen(fullpath, "rb");
    if (!f) {
        send_response(client, "404 Not Found", "text/plain", "File not found", NULL);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size);
    fread(content, 1, size, f);
    fclose(f);
    
    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Content-Length: %ld\r\n\r\n", strrchr(path, '/') + 1, size);
    
    write(client, header, strlen(header));
    write(client, content, size);
    free(content);
}

void handle_request(int client) {
    char buffer[BUFFER_SIZE];
    int n = read(client, buffer, sizeof(buffer) - 1);
    if (n <= 0) return;
    buffer[n] = '\0';
    
    char method[16], path[512], *cookie = NULL;
    sscanf(buffer, "%s %s", method, path);
    
    char *cookie_line = strstr(buffer, "Cookie:");
    if (cookie_line) {
        char *token_start = strstr(cookie_line, "session=");
        if (token_start) {
            token_start += 8;
            cookie = strndup(token_start, 32);
        }
    }
    
    if (strcmp(path, "/login") == 0 && strcmp(method, "POST") == 0) {
        char *body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            char username[64] = {0}, password[64] = {0};
            sscanf(body, "username=%[^&]&password=%s", username, password);
            
            if (strcmp(username, "m5rcel") == 0 && strcmp(password, "M%40rc8l1257") == 0) {
                char *token = generate_token();
                create_session(token);
                char cookie_header[256];
                snprintf(cookie_header, sizeof(cookie_header), "Set-Cookie: session=%s; Path=/; Max-Age=3600\r\n", token);
                send_response(client, "302 Found", "text/html", "<html><body>Redirecting...</body></html>", 
                    "Location: /\r\nSet-Cookie: session=" );
                char redirect[512];
                snprintf(redirect, sizeof(redirect), 
                    "HTTP/1.1 302 Found\r\nLocation: /\r\nSet-Cookie: session=%s; Path=/; Max-Age=3600\r\n\r\n", token);
                write(client, redirect, strlen(redirect));
                free(cookie);
                return;
            }
        }
        send_login_page(client);
    } else if (strcmp(path, "/logout") == 0) {
        send_response(client, "302 Found", "text/html", "", "Location: /\r\nSet-Cookie: session=; Max-Age=0\r\n");
    } else if (!cookie || !verify_session(cookie)) {
        send_login_page(client);
    } else if (strcmp(path, "/") == 0) {
        send_main_page(client, cookie);
    } else if (strncmp(path, "/files", 6) == 0) {
        send_files_page(client, path + 6);
    } else if (strncmp(path, "/raw", 4) == 0) {
        send_raw_file(client, path + 4);
    } else if (strcmp(path, "/download") == 0) {
        system("cd /home/m5rcel/Fit && tar czf /tmp/fit-repo.tar.gz --exclude=.fit .");
        FILE *f = fopen("/tmp/fit-repo.tar.gz", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *data = malloc(size);
            fread(data, 1, size, f);
            fclose(f);
            
            char header[256];
            snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\nContent-Type: application/gzip\r\n"
                "Content-Disposition: attachment; filename=\"fit-repo.tar.gz\"\r\n"
                "Content-Length: %ld\r\n\r\n", size);
            write(client, header, strlen(header));
            write(client, data, size);
            free(data);
        }
    } else {
        send_response(client, "404 Not Found", "text/plain", "Not Found", NULL);
    }
    
    if (cookie) free(cookie);
}

int main() {
    init_db();
    
    int server = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    bind(server, (struct sockaddr*)&addr, sizeof(addr));
    listen(server, 10);
    
    printf("Fit Web Interface running on http://localhost:%d\n", PORT);
    printf("Login: m5rcel / M@rc8l1257\n");
    
    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) continue;
        handle_request(client);
        close(client);
    }
    
    sqlite3_close(db);
    return 0;
}
