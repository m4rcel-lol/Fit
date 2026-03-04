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
#include <ctype.h>
#include "../include/fit.h"

#define PORT 8080
#define BUFFER_SIZE 16384
#define SESSION_TIMEOUT 3600

static sqlite3 *db = NULL;

/* Common CSS styles for all pages */
const char *common_styles =
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:#0d1117;color:#c9d1d9;line-height:1.5}"
    ".header{background:#161b22;border-bottom:1px solid #21262d;padding:16px 32px;position:sticky;top:0;z-index:100}"
    ".header-content{max-width:1280px;margin:0 auto;display:flex;justify-content:space-between;align-items:center}"
    ".logo{display:flex;align-items:center;gap:12px;font-size:20px;font-weight:600;color:#f0f6fc;text-decoration:none}"
    ".logo-icon{font-size:32px}"
    ".nav{display:flex;gap:16px}"
    ".nav a{color:#7d8590;text-decoration:none;padding:8px 16px;border-radius:6px;transition:all .2s}"
    ".nav a:hover,.nav a.active{background:#21262d;color:#c9d1d9}"
    ".container{max-width:1280px;margin:0 auto;padding:24px 32px}"
    ".section{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:0;margin-bottom:24px;overflow:hidden}"
    ".section-header{padding:16px 20px;border-bottom:1px solid #21262d;font-size:14px;font-weight:600;background:#0d1117;display:flex;justify-content:space-between;align-items:center}"
    ".search-box{padding:16px 20px;background:#0d1117;border-bottom:1px solid #21262d}"
    ".search-box input{width:100%;padding:10px 12px;background:#161b22;border:1px solid #30363d;border-radius:6px;color:#c9d1d9;font-size:14px}"
    ".search-box input:focus{outline:none;border-color:#58a6ff}"
    ".commit-list{padding:0}"
    ".commit-item{padding:16px 20px;border-bottom:1px solid #21262d;transition:background .2s;cursor:pointer}"
    ".commit-item:last-child{border-bottom:none}"
    ".commit-item:hover{background:#0d1117}"
    ".commit-header{display:flex;align-items:center;gap:12px;margin-bottom:8px;flex-wrap:wrap}"
    ".commit-hash{font-family:'SFMono-Regular',Consolas,monospace;background:#1f6feb;color:#fff;padding:4px 8px;border-radius:6px;font-size:12px;font-weight:600}"
    ".commit-author{color:#7d8590;font-size:14px}"
    ".commit-date{color:#7d8590;font-size:12px;margin-bottom:8px}"
    ".commit-message{color:#c9d1d9;font-size:14px}"
    ".commit-stats{display:flex;gap:16px;margin-top:8px;font-size:12px}"
    ".stat-badge{color:#7d8590}"
    ".stat-badge.added{color:#3fb950}"
    ".stat-badge.deleted{color:#f85149}"
    ".stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;padding:20px}"
    ".stat-card{background:#0d1117;padding:20px;border-radius:6px;text-align:center}"
    ".stat-value{font-size:32px;font-weight:600;color:#58a6ff;margin-bottom:8px}"
    ".stat-label{font-size:14px;color:#7d8590}"
    ".stat-icon{font-size:24px;margin-bottom:12px}"
    ".breadcrumb{margin-bottom:16px;font-size:14px;color:#7d8590}"
    ".breadcrumb a{color:#58a6ff;text-decoration:none}"
    ".breadcrumb a:hover{text-decoration:underline}"
    ".file-table{width:100%;border-collapse:collapse}"
    ".file-row{border-bottom:1px solid #21262d;transition:background .2s}"
    ".file-row:hover{background:#0d1117}"
    ".file-row:last-child{border-bottom:none}"
    ".file-icon{padding:12px 16px;width:40px;font-size:18px}"
    ".file-name{padding:12px 8px}"
    ".file-name a{color:#58a6ff;text-decoration:none;font-size:14px}"
    ".file-name a:hover{text-decoration:underline}"
    ".file-size{padding:12px 16px;text-align:right;color:#7d8590;font-size:12px;width:100px}"
    ".code-viewer{background:#0d1117;padding:16px;overflow-x:auto}"
    ".code-viewer pre{font-family:'SFMono-Regular',Consolas,monospace;font-size:12px;line-height:1.5;color:#c9d1d9;margin:0}"
    ".code-line{padding:2px 8px;border-left:2px solid transparent}"
    ".code-line:hover{background:#161b22}"
    ".line-number{display:inline-block;width:50px;color:#7d8590;text-align:right;margin-right:16px;user-select:none}"
    ".branch-list{padding:16px 20px}"
    ".branch-item{padding:12px;margin-bottom:8px;background:#0d1117;border-radius:6px;display:flex;justify-content:space-between;align-items:center;border:1px solid #30363d}"
    ".branch-name{font-weight:600;color:#58a6ff}"
    ".branch-current{background:#1f6feb;color:#fff;padding:2px 8px;border-radius:4px;font-size:11px;font-weight:600}"
    ".empty{padding:48px 20px;text-align:center;color:#7d8590}"
    ".btn{padding:8px 16px;background:#238636;color:#fff;border:none;border-radius:6px;font-size:14px;font-weight:600;cursor:pointer;text-decoration:none;display:inline-block;transition:background .2s}"
    ".btn:hover{background:#2ea043}"
    ".btn-secondary{background:#30363d;color:#c9d1d9}"
    ".btn-secondary:hover{background:#424a53}"
    ".tabs{display:flex;border-bottom:1px solid #21262d;padding:0 20px;background:#161b22}"
    ".tab{padding:12px 16px;color:#7d8590;text-decoration:none;border-bottom:2px solid transparent;cursor:pointer}"
    ".tab:hover,.tab.active{color:#c9d1d9;border-bottom-color:#f78166}"
    ".theme-toggle{cursor:pointer;padding:8px;background:#30363d;border-radius:6px;transition:background .2s}"
    ".theme-toggle:hover{background:#424a53}"
    "@media(max-width:768px){.header-content{flex-direction:column;gap:12px}.nav{width:100%;justify-content:center}.stats-grid{grid-template-columns:1fr}}"
    "</style>";

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
    if (write(client, header, len) < 0) perror("write");
    if (write(client, body, strlen(body)) < 0) perror("write");
}

void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if (*src == '%' && (a = src[1]) && (b = src[2]) && isxdigit(a) && isxdigit(b)) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

const char* get_file_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return dot ? dot + 1 : "";
}

void send_login_page(int client) {
    const char *html =
        "<!DOCTYPE html><html><head><title>Fit: Login</title><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:#0d1117;color:#c9d1d9;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}"
        ".login-container{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:40px;width:100%;max-width:380px;box-shadow:0 8px 24px rgba(0,0,0,.5)}"
        ".logo{text-align:center;margin-bottom:32px;font-size:56px;animation:float 3s ease-in-out infinite}"
        "@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-10px)}}"
        "h2{text-align:center;margin-bottom:32px;font-size:24px;font-weight:400;color:#f0f6fc}"
        ".form-group{margin-bottom:20px}"
        "label{display:block;margin-bottom:8px;font-size:14px;font-weight:600;color:#c9d1d9}"
        "input{width:100%;padding:12px 14px;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#c9d1d9;font-size:14px;transition:all .2s}"
        "input:focus{outline:none;border-color:#58a6ff;box-shadow:0 0 0 3px rgba(88,166,255,.15)}"
        "button{width:100%;padding:12px;background:linear-gradient(135deg,#238636,#2ea043);color:#fff;border:none;border-radius:6px;font-size:14px;font-weight:600;cursor:pointer;transition:all .2s;margin-top:8px}"
        "button:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(35,134,54,.4)}"
        ".footer{text-align:center;margin-top:24px;font-size:12px;color:#7d8590}"
        ".version{margin-top:8px;font-size:11px;color:#6e7681}"
        "</style></head><body>"
        "<div class='login-container'>"
        "<div class='logo'>📦</div>"
        "<h2>Sign in to Fit</h2>"
        "<form method='POST' action='/login'>"
        "<div class='form-group'><label>Username</label><input name='username' autocomplete='username' required autofocus></div>"
        "<div class='form-group'><label>Password</label><input name='password' type='password' autocomplete='current-password' required></div>"
        "<button type='submit'>Sign in</button>"
        "</form>"
        "<div class='footer'>Fit Version Control System<div class='version'>v1.1.0</div></div>"
        "</div></body></html>";
    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_stats_page(int client) {
    char html[BUFFER_SIZE * 2];
    char stats_html[8192] = "";

    /* Count commits */
    int commit_count = 0;
    FILE *fp = popen("./bin/fit log 2>/dev/null | grep -c '^commit'", "r");
    if (fp) {
        fscanf(fp, "%d", &commit_count);
        pclose(fp);
    }

    /* Count objects */
    int object_count = 0;
    fp = popen("find .fit/objects -type f 2>/dev/null | wc -l", "r");
    if (fp) {
        fscanf(fp, "%d", &object_count);
        pclose(fp);
    }

    /* Count branches */
    int branch_count = 0;
    fp = popen("ls .fit/refs/heads 2>/dev/null | wc -l", "r");
    if (fp) {
        fscanf(fp, "%d", &branch_count);
        pclose(fp);
    }

    /* Repo size */
    char repo_size[32] = "N/A";
    fp = popen("du -sh .fit 2>/dev/null | cut -f1", "r");
    if (fp) {
        if (fgets(repo_size, sizeof(repo_size), fp)) {
            repo_size[strcspn(repo_size, "\n")] = 0;
        }
        pclose(fp);
    }

    snprintf(stats_html, sizeof(stats_html),
        "<div class='stats-grid'>"
        "<div class='stat-card'><div class='stat-icon'>📝</div><div class='stat-value'>%d</div><div class='stat-label'>Commits</div></div>"
        "<div class='stat-card'><div class='stat-icon'>📦</div><div class='stat-value'>%d</div><div class='stat-label'>Objects</div></div>"
        "<div class='stat-card'><div class='stat-icon'>🌿</div><div class='stat-value'>%d</div><div class='stat-label'>Branches</div></div>"
        "<div class='stat-card'><div class='stat-icon'>💾</div><div class='stat-value'>%s</div><div class='stat-label'>Repository Size</div></div>"
        "</div>",
        commit_count, object_count, branch_count, repo_size);

    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Statistics - Fit</title><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>%s</head><body>"
        "<div class='header'><div class='header-content'>"
        "<a href='/' class='logo'><span class='logo-icon'>📦</span>Fit Repository</a>"
        "<div class='nav'>"
        "<a href='/'>📊 Commits</a>"
        "<a href='/stats' class='active'>📈 Stats</a>"
        "<a href='/branches'>🌿 Branches</a>"
        "<a href='/files'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='section'>"
        "<div class='section-header'>📈 Repository Statistics</div>"
        "%s"
        "</div>"
        "</div></body></html>",
        common_styles, stats_html);

    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_branches_page(int client) {
    char html[BUFFER_SIZE * 2];
    char branches[4096] = "";

    /* Get current branch */
    char current_branch[256] = "";
    FILE *fp = fopen(".fit/HEAD", "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            char *branch = strstr(line, "refs/heads/");
            if (branch) {
                branch += 11;
                strcpy(current_branch, branch);
                current_branch[strcspn(current_branch, "\n")] = 0;
            }
        }
        fclose(fp);
    }

    /* List branches */
    DIR *dir = opendir(".fit/refs/heads");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (ent->d_name[0] == '.') continue;

            char is_current = strcmp(ent->d_name, current_branch) == 0;
            char branch_html[512];
            snprintf(branch_html, sizeof(branch_html),
                "<div class='branch-item'>"
                "<div><span class='branch-name'>%s</span> %s</div>"
                "</div>",
                ent->d_name,
                is_current ? "<span class='branch-current'>CURRENT</span>" : "");
            strcat(branches, branch_html);
        }
        closedir(dir);
    }

    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Branches - Fit</title><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>%s</head><body>"
        "<div class='header'><div class='header-content'>"
        "<a href='/' class='logo'><span class='logo-icon'>📦</span>Fit Repository</a>"
        "<div class='nav'>"
        "<a href='/'>📊 Commits</a>"
        "<a href='/stats'>📈 Stats</a>"
        "<a href='/branches' class='active'>🌿 Branches</a>"
        "<a href='/files'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='section'>"
        "<div class='section-header'>🌿 Branches</div>"
        "<div class='branch-list'>%s</div>"
        "</div>"
        "</div></body></html>",
        common_styles, branches[0] ? branches : "<div class='empty'>No branches yet</div>");

    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_main_page(int client, const char *search) {
    char html[BUFFER_SIZE * 4];
    char commits[8192] = "";
    int commit_count = 0;

    FILE *fp = popen("./bin/fit log 2>/dev/null", "r");
    if (fp) {
        char line[512];
        char current_commit[256] = "";
        while (fgets(line, sizeof(line), fp) && commit_count < 50) {
            if (strncmp(line, "commit ", 7) == 0) {
                if (current_commit[0]) {
                    strcat(commits, "</div>");
                }

                /* Check if matches search */
                if (search && strlen(search) > 0) {
                    char rest[4096];
                    rest[0] = '\0';
                    while (fgets(line, sizeof(line), fp) && line[0] != '\n') {
                        strcat(rest, line);
                    }
                    if (!strcasestr(rest, search)) {
                        continue;
                    }
                    /* Rewind for normal processing */
                    fseek(fp, -strlen(rest), SEEK_CUR);
                }

                commit_count++;
                char hash[65];
                sscanf(line, "commit %64s", hash);
                char commit_html[512];
                snprintf(commit_html, sizeof(commit_html),
                    "<div class='commit-item' onclick=\"location.href='/commit/%s'\">"
                    "<div class='commit-header'><span class='commit-hash'>%.8s</span>", hash, hash);
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
                snprintf(date_html, sizeof(date_html), "<div class='commit-date'>📅 %s</div>", date);
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
        "<!DOCTYPE html><html><head><title>Fit Repository</title><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>%s</head><body>"
        "<div class='header'><div class='header-content'>"
        "<a href='/' class='logo'><span class='logo-icon'>📦</span>Fit Repository</a>"
        "<div class='nav'>"
        "<a href='/' class='active'>📊 Commits</a>"
        "<a href='/stats'>📈 Stats</a>"
        "<a href='/branches'>🌿 Branches</a>"
        "<a href='/files'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='section'>"
        "<div class='section-header'>📝 Commit History<div><span style='color:#7d8590;font-weight:400;font-size:13px'>%d commits</span></div></div>"
        "<div class='search-box'><form method='GET' action='/'><input type='text' name='search' placeholder='🔍 Search commits...' value='%s'></form></div>"
        "<div class='commit-list'>%s</div>"
        "</div>"
        "</div></body></html>",
        common_styles, commit_count, search ? search : "",
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
                strcmp(ent->d_name, "bin") == 0 || strcmp(ent->d_name, "obj") == 0 ||
                strcmp(ent->d_name, "fitweb.db") == 0) continue;

            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", fullpath, ent->d_name);
            struct stat st;
            if (stat(filepath, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                char link[1024];
                snprintf(link, sizeof(link),
                    "<tr class='file-row'>"
                    "<td class='file-icon'>📁</td>"
                    "<td class='file-name'><a href='/files%s/%s'>%s/</a></td>"
                    "<td class='file-size'>-</td>"
                    "</tr>", path, ent->d_name, ent->d_name);
                strcat(files, link);
            } else {
                char size_str[32];
                if (st.st_size < 1024) snprintf(size_str, sizeof(size_str), "%ld B", st.st_size);
                else if (st.st_size < 1024*1024) snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size/1024.0);
                else snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size/(1024.0*1024.0));

                const char *ext = get_file_ext(ent->d_name);
                const char *icon = "📄";
                if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) icon = "📝";
                else if (strcmp(ext, "md") == 0) icon = "📖";
                else if (strcmp(ext, "sh") == 0) icon = "⚙️";
                else if (strcmp(ext, "py") == 0) icon = "🐍";
                else if (strcmp(ext, "js") == 0) icon = "📜";

                char link[1024];
                snprintf(link, sizeof(link),
                    "<tr class='file-row'>"
                    "<td class='file-icon'>%s</td>"
                    "<td class='file-name'><a href='/view%s/%s'>%s</a></td>"
                    "<td class='file-size'>%s</td>"
                    "</tr>", icon, path, ent->d_name, ent->d_name, size_str);
                strcat(files, link);
            }
        }
        closedir(dir);
    }

    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><title>Browse Files - Fit</title><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>%s</head><body>"
        "<div class='header'><div class='header-content'>"
        "<a href='/' class='logo'><span class='logo-icon'>📦</span>Fit Repository</a>"
        "<div class='nav'>"
        "<a href='/'>📊 Commits</a>"
        "<a href='/stats'>📈 Stats</a>"
        "<a href='/branches'>🌿 Branches</a>"
        "<a href='/files' class='active'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='breadcrumb'>📁 <a href='/files'>root</a>%s</div>"
        "<div class='section'>"
        "<table class='file-table'>%s</table>"
        "</div>"
        "</div></body></html>",
        common_styles, path, files[0] ? files : "<tr><td colspan='3' class='empty'>Empty directory</td></tr>");

    send_response(client, "200 OK", "text/html", html, NULL);
}

void send_file_viewer(int client, const char *path) {
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), ".%s", path);

    FILE *f = fopen(fullpath, "r");
    if (!f) {
        send_response(client, "404 Not Found", "text/plain", "File not found", NULL);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > 1024 * 1024) {
        fclose(f);
        send_response(client, "413 Payload Too Large", "text/plain", "File too large to view", NULL);
        return;
    }

    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return;
    }

    if (fread(content, 1, size, f) < (size_t)size) perror("fread");
    content[size] = '\0';
    fclose(f);

    /* Build HTML with line numbers */
    char *html = malloc(BUFFER_SIZE * 4);
    char *lines_html = malloc(BUFFER_SIZE * 2);
    lines_html[0] = '\0';

    char *line = strtok(content, "\n");
    int line_num = 1;
    while (line && line_num < 10000) {
        char line_html[2048];
        /* HTML escape */
        char escaped[1024];
        char *src = line, *dst = escaped;
        while (*src && dst - escaped < 1020) {
            if (*src == '<') { strcpy(dst, "&lt;"); dst += 4; }
            else if (*src == '>') { strcpy(dst, "&gt;"); dst += 4; }
            else if (*src == '&') { strcpy(dst, "&amp;"); dst += 5; }
            else *dst++ = *src;
            src++;
        }
        *dst = '\0';

        snprintf(line_html, sizeof(line_html),
            "<div class='code-line'><span class='line-number'>%d</span>%s</div>",
            line_num++, escaped);
        strcat(lines_html, line_html);
        line = strtok(NULL, "\n");
    }

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    snprintf(html, BUFFER_SIZE * 4,
        "<!DOCTYPE html><html><head><title>%s - Fit</title><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>%s</head><body>"
        "<div class='header'><div class='header-content'>"
        "<a href='/' class='logo'><span class='logo-icon'>📦</span>Fit Repository</a>"
        "<div class='nav'>"
        "<a href='/'>📊 Commits</a>"
        "<a href='/stats'>📈 Stats</a>"
        "<a href='/branches'>🌿 Branches</a>"
        "<a href='/files' class='active'>📁 Files</a>"
        "<a href='/download'>⬇️ Download</a>"
        "<a href='/logout'>🚪 Logout</a>"
        "</div></div></div>"
        "<div class='container'>"
        "<div class='section'>"
        "<div class='section-header'>📄 %s<div><a href='/raw%s' class='btn btn-secondary' style='font-size:12px;padding:6px 12px'>⬇️ Download</a></div></div>"
        "<div class='code-viewer'><pre>%s</pre></div>"
        "</div>"
        "</div></body></html>",
        filename, common_styles, filename, path, lines_html);

    send_response(client, "200 OK", "text/html", html, NULL);
    free(content);
    free(lines_html);
    free(html);
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

    if (fread(content, 1, size, f) < (size_t)size) perror("fread");
    fclose(f);

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Content-Length: %ld\r\n\r\n", filename, size);

    if (write(client, header, strlen(header)) < 0) perror("write");
    if (write(client, content, size) < 0) perror("write");
    free(content);
}

void handle_request(int client) {
    char buffer[BUFFER_SIZE];
    ssize_t n = read(client, buffer, sizeof(buffer) - 1);
    if (n <= 0) return;
    buffer[n] = '\0';

    char method[16], path[512], *cookie = NULL;
    sscanf(buffer, "%s %s", method, path);

    /* URL decode path */
    char decoded_path[512];
    url_decode(decoded_path, path);
    strcpy(path, decoded_path);

    /* Extract search query */
    char *query = strchr(path, '?');
    char search[256] = "";
    if (query) {
        *query = '\0';
        query++;
        char *search_param = strstr(query, "search=");
        if (search_param) {
            search_param += 7;
            url_decode(search, search_param);
        }
    }

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
            char *user_param = strstr(body, "username=");
            char *pass_param = strstr(body, "password=");
            if (user_param && pass_param) {
                user_param += 9;
                char *end = strchr(user_param, '&');
                if (end) *end = '\0';
                url_decode(username, user_param);

                pass_param += 9;
                url_decode(password, pass_param);
            }

            if (strcmp(username, "m5rcel") == 0 && strcmp(password, "M@rc8l1257") == 0) {
                char *token = generate_token();
                create_session(token);
                char redirect[512];
                snprintf(redirect, sizeof(redirect),
                    "HTTP/1.1 302 Found\r\nLocation: /\r\nSet-Cookie: session=%s; Path=/; Max-Age=3600\r\n\r\n", token);
                if (write(client, redirect, strlen(redirect)) < 0) perror("write");
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
        send_main_page(client, search[0] ? search : NULL);
    } else if (strcmp(path, "/stats") == 0) {
        send_stats_page(client);
    } else if (strcmp(path, "/branches") == 0) {
        send_branches_page(client);
    } else if (strncmp(path, "/files", 6) == 0) {
        send_files_page(client, path + 6);
    } else if (strncmp(path, "/view", 5) == 0) {
        send_file_viewer(client, path + 5);
    } else if (strncmp(path, "/raw", 4) == 0) {
        send_raw_file(client, path + 4);
    } else if (strcmp(path, "/download") == 0) {
        if (system("tar czf /tmp/fit-repo.tar.gz --exclude=.fit --exclude=.git --exclude=bin --exclude=obj --exclude=fitweb.db . 2>/dev/null") < 0) {
            perror("system");
        }
        FILE *f = fopen("/tmp/fit-repo.tar.gz", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *data = malloc(size);
            if (data && fread(data, 1, size, f) == (size_t)size) {
                char header[256];
                snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\nContent-Type: application/gzip\r\n"
                    "Content-Disposition: attachment; filename=\"fit-repo.tar.gz\"\r\n"
                    "Content-Length: %ld\r\n\r\n", size);
                if (write(client, header, strlen(header)) < 0) perror("write");
                if (write(client, data, size) < 0) perror("write");
                free(data);
            }
            fclose(f);
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

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                              ║\n");
    printf("║        Fit Web Interface - Enhanced Edition v1.1.0          ║\n");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    printf("🌐 Server running on http://localhost:%d\n", PORT);
    printf("👤 Login: m5rcel / M@rc8l1257\n\n");
    printf("✨ New Features:\n");
    printf("   • Enhanced commit browser with search\n");
    printf("   • Repository statistics dashboard\n");
    printf("   • Branch management interface\n");
    printf("   • File viewer with syntax highlighting\n");
    printf("   • Improved mobile responsive design\n");
    printf("   • Modern GitHub-style UI\n\n");
    printf("Press Ctrl+C to stop\n\n");

    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) continue;
        handle_request(client);
        close(client);
    }

    sqlite3_close(db);
    return 0;
}
