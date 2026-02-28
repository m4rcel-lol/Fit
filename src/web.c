#define _POSIX_C_SOURCE 200809L
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
        "<!DOCTYPE html><html><head><title>Fit: Login</title><meta charset='utf-8'>"
        "<style>*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:#0d1117;color:#c9d1d9;min-height:100vh;display:flex;align-items:center;justify-content:center}"
        ".login-container{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:32px;width:340px;box-shadow:0 8px 24px rgba(0,0,0,.4)}"
        ".logo{text-align:center;margin-bottom:24px;font-size:48px;color:#58a6ff}"
        "h2{text-align:center;margin-bottom:24px;font-size:24px;font-weight:300;color:#f0f6fc}"
        ".form-group{margin-bottom:16px}"
        "label{display:block;margin-bottom:8px;font-size:14px;font-weight:600;color:#c9d1d9}"
        "input{width:100%;padding:10px 12px;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#c9d1d9;font-size:14px;transition:border-color .2s}"
        "input:focus{outline:none;border-color:#58a6ff;box-shadow:0 0 0 3px rgba(88,166,255,.1)}"
        "button{width:100%;padding:10px;background:#238636;color:#fff;border:none;border-radius:6px;font-size:14px;font-weight:600;cursor:pointer;transition:background .2s}"
        "button:hover{background:#2ea043}"
        ".footer{text-align:center;margin-top:16px;font-size:12px;color:#8b949e}"
        "</style></head><body>"
        "<div class='login-container'>"
        "<div class='logo'>📦</div>"
        "<h2>Sign in to Fit</h2>"
        "<form method='POST' action='/login'>"
        "<div class='form-group'><label>Username</label><input name='username' autocomplete='username' required></div>"
        "<div class='form-group'><label>Password</label><input name='password' type='password' autocomplete='current-password' required></div>"
        "<button type='submit'>Sign in</button>"
        "</form>"
        "<div class='footer'>Fit Version Control System</div>"
        "</div></body></html>";
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_main_page(int client, const char *token __attribute__((unused))) {
    char html[BUFFER_SIZE * 2];
    char commits[4096] = "";
    int commit_count = 0;
    
    FILE *fp = popen("./bin/fit log 2>/dev/null", "r");
    if (fp) {
        char line[512];
        char current_commit[256] = "";
        while (fgets(line, sizeof(line), fp) && commit_count < 20) {
            if (strncmp(line, "commit ", 7) == 0) {
                if (current_commit[0]) {
                    strcat(commits, "</div>");
                }
                commit_count++;
                char hash[65];
                sscanf(line, "commit %64s", hash);
                char commit_html[512];
                snprintf(commit_html, sizeof(commit_html),
                    "<div class='commit-item'>"
                    "<div class='commit-header'><span class='commit-hash'>%.8s</span>", hash);
                strcat(commits, commit_html);
                strcpy(current_commit, hash);
            } else if (strstr(line, "Author:")) {
                char author[128];
                sscanf(line, " Author: %127[^\n]", author);
                char author_html[256];
                snprintf(author_html, sizeof(author_html), "<span class='commit-author'>%s</span></div>", author);
                strcat(commits, author_html);
            } else if (strstr(line, "Date:")) {
                char date[128];
                sscanf(line, " Date: %127[^\n]", date);
                char date_html[256];
                snprintf(date_html, sizeof(date_html), "<div class='commit-date'>%s</div>", date);
                strcat(commits, date_html);
            } else if (line[0] != '\n' && line[0] != ' ' && line[0] != '\t') {
                continue;
            } else {
                char msg[256];
                if (sscanf(line, " %255[^\n]", msg) == 1) {
                    char msg_html[512];
                    snprintf(msg_html, sizeof(msg_html), "<div class='commit-message'>%s</div>", msg);
                    strcat(commits, msg_html);
                }
            }
        }
        if (current_commit[0]) strcat(commits, "</div>");
        pclose(fp);
    }
    
    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Fit Repository</title><meta charset='utf-8'>"
        "<style>*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:#0d1117;color:#c9d1d9;line-height:1.5}"
        ".header{background:#161b22;border-bottom:1px solid #21262d;padding:16px 32px}"
        ".header-content{max-width:1280px;margin:0 auto;display:flex;justify-content:space-between;align-items:center}"
        ".logo{display:flex;align-items:center;gap:12px;font-size:20px;font-weight:600;color:#f0f6fc}"
        ".logo-icon{font-size:32px}"
        ".nav{display:flex;gap:16px}"
        ".nav a{color:#7d8590;text-decoration:none;padding:8px 16px;border-radius:6px;transition:all .2s}"
        ".nav a:hover{background:#21262d;color:#c9d1d9}"
        ".container{max-width:1280px;margin:0 auto;padding:24px 32px}"
        ".section{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:0;margin-bottom:24px;overflow:hidden}"
        ".section-header{padding:16px 20px;border-bottom:1px solid #21262d;font-size:14px;font-weight:600;background:#0d1117}"
        ".commit-list{padding:0}"
        ".commit-item{padding:16px 20px;border-bottom:1px solid #21262d;transition:background .2s}"
        ".commit-item:last-child{border-bottom:none}"
        ".commit-item:hover{background:#0d1117}"
        ".commit-header{display:flex;align-items:center;gap:12px;margin-bottom:8px}"
        ".commit-hash{font-family:'SFMono-Regular',Consolas,monospace;background:#1f6feb;color:#fff;padding:4px 8px;border-radius:6px;font-size:12px;font-weight:600}"
        ".commit-author{color:#7d8590;font-size:14px}"
        ".commit-date{color:#7d8590;font-size:12px;margin-bottom:8px}"
        ".commit-message{color:#c9d1d9;font-size:14px}"
        ".stats{display:flex;gap:24px;padding:20px}"
        ".stat-item{text-align:center}"
        ".stat-value{font-size:24px;font-weight:600;color:#58a6ff}"
        ".stat-label{font-size:12px;color:#7d8590;margin-top:4px}"
        ".empty{padding:48px 20px;text-align:center;color:#7d8590}"
        "</style></head><body>"
        "<div class='header'><div class='header-content'>"
        "<div class='logo'><span class='logo-icon'>📦</span>Fit Repository</div>"
        "<div class='nav'>"
        "<a href='/'>📊 Commits</a>"
        "<a href='/files'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='section'>"
        "<div class='section-header'>📝 Recent Commits</div>"
        "<div class='commit-list'>%s</div>"
        "</div>"
        "</div></body></html>",
        commits[0] ? commits : "<div class='empty'>No commits yet. Initialize repository with 'fit init' and create your first commit.</div>");
    
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_files_page(int client, const char *path) {
    char html[BUFFER_SIZE * 2];
    char files[8192] = "";
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), ".%s", path[0] ? path : "");
    
    DIR *dir = opendir(fullpath);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (strstr(ent->d_name, ".fit") || strstr(ent->d_name, ".git") || 
                strcmp(ent->d_name, "bin") == 0 || strcmp(ent->d_name, "obj") == 0) continue;
            
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", fullpath, ent->d_name);
            struct stat st;
            if (stat(filepath, &st) != 0) continue;
            
            if (S_ISDIR(st.st_mode)) {
                char link[1024];
                snprintf(link, sizeof(link), 
                    "<tr class='file-row'>"
                    "<td class='file-icon'>📁</td>"
                    "<td class='file-name'><a href='/files%s/%s'>%s</a></td>"
                    "<td class='file-size'>-</td>"
                    "</tr>", path, ent->d_name, ent->d_name);
                strcat(files, link);
            } else {
                char size_str[32];
                if (st.st_size < 1024) snprintf(size_str, sizeof(size_str), "%ld B", st.st_size);
                else if (st.st_size < 1024*1024) snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size/1024.0);
                else snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size/(1024.0*1024.0));
                
                char link[1024];
                snprintf(link, sizeof(link),
                    "<tr class='file-row'>"
                    "<td class='file-icon'>📄</td>"
                    "<td class='file-name'><a href='/raw%s/%s'>%s</a></td>"
                    "<td class='file-size'>%s</td>"
                    "</tr>", path, ent->d_name, ent->d_name, size_str);
                strcat(files, link);
            }
        }
        closedir(dir);
    }
    
    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Browse Files - Fit</title><meta charset='utf-8'>"
        "<style>*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:#0d1117;color:#c9d1d9;line-height:1.5}"
        ".header{background:#161b22;border-bottom:1px solid #21262d;padding:16px 32px}"
        ".header-content{max-width:1280px;margin:0 auto;display:flex;justify-content:space-between;align-items:center}"
        ".logo{display:flex;align-items:center;gap:12px;font-size:20px;font-weight:600;color:#f0f6fc}"
        ".logo-icon{font-size:32px}"
        ".nav{display:flex;gap:16px}"
        ".nav a{color:#7d8590;text-decoration:none;padding:8px 16px;border-radius:6px;transition:all .2s}"
        ".nav a:hover{background:#21262d;color:#c9d1d9}"
        ".container{max-width:1280px;margin:0 auto;padding:24px 32px}"
        ".breadcrumb{margin-bottom:16px;font-size:14px;color:#7d8590}"
        ".breadcrumb a{color:#58a6ff;text-decoration:none}"
        ".breadcrumb a:hover{text-decoration:underline}"
        ".section{background:#161b22;border:1px solid #30363d;border-radius:6px;overflow:hidden}"
        ".file-table{width:100%%;border-collapse:collapse}"
        ".file-row{border-bottom:1px solid #21262d;transition:background .2s}"
        ".file-row:hover{background:#0d1117}"
        ".file-row:last-child{border-bottom:none}"
        ".file-icon{padding:12px 16px;width:40px;font-size:18px}"
        ".file-name{padding:12px 8px}"
        ".file-name a{color:#58a6ff;text-decoration:none;font-size:14px}"
        ".file-name a:hover{text-decoration:underline}"
        ".file-size{padding:12px 16px;text-align:right;color:#7d8590;font-size:12px;width:100px}"
        ".empty{padding:48px 20px;text-align:center;color:#7d8590}"
        "</style></head><body>"
        "<div class='header'><div class='header-content'>"
        "<div class='logo'><span class='logo-icon'>📦</span>Fit Repository</div>"
        "<div class='nav'>"
        "<a href='/'>📊 Commits</a>"
        "<a href='/files'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='breadcrumb'>📁 <a href='/files'>root</a>%s</div>"
        "<div class='section'>"
        "<table class='file-table'>%s</table>"
        "</div>"
        "</div></body></html>",
        path, files[0] ? files : "<tr><td colspan='3' class='empty'>Empty directory</td></tr>");
    
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_raw_file(int client, const char *path) {
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), ".%s", path);
    
    FILE *f = fopen(fullpath, "rb");
    if (!f) {
        send_response(client, "404 Not Found", "text/plain", "File not found", NULL);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        send_response(client, "500 Internal Server Error", "text/plain", "Out of memory", NULL);
        return;
    }
    
    fread(content, 1, size, f);
    fclose(f);
    
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    
    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Content-Length: %ld\r\n\r\n", filename, size);
    
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
        system("tar czf /tmp/fit-repo.tar.gz --exclude=.fit --exclude=.git --exclude=bin --exclude=obj .");
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
