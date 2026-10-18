 #include "http_server.h"
 #include "common.h"
 #include "storage.h"
 #include "session.h"
 #include "logger.h"
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <ctype.h>
 #include <pthread.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <stdint.h>
 
 /* ---------- вспомогательные функции ---------- */
 
 /* URL-декодирование: %XX -> символ, + -> пробел */
 static void url_decode(const char *src, char *dst, size_t dst_size) {
     if (!src || !dst) return;
     
     size_t i = 0, j = 0;
     while (src[i] && j < dst_size - 1) {
         if (src[i] == '%' && src[i+1] && src[i+2]) {
             char hex[3] = {src[i+1], src[i+2], 0};
             int val = (int)strtol(hex, NULL, 16);
             dst[j++] = (char)val;
             i += 3;
         } else if (src[i] == '+') {
             dst[j++] = ' ';
             i++;
         } else {
             dst[j++] = src[i++];
         }
     }
     dst[j] = '\0';
 }
 
 /* Экранирование спецсимволов для JSON */
 static void json_escape(const char *src, char *dst, size_t dst_size) {
     if (!src || !dst) return;
     
     size_t i = 0, j = 0;
     while (src[i] && j < dst_size - 2) {
         if (src[i] == '"') {
             dst[j++] = '\\';
             dst[j++] = '"';
         } else if (src[i] == '\\') {
             dst[j++] = '\\';
             dst[j++] = '\\';
         } else if (src[i] == '\n') {
             dst[j++] = '\\';
             dst[j++] = 'n';
         } else if (src[i] == '\r') {
             dst[j++] = '\\';
             dst[j++] = 'r';
         } else if (src[i] == '\t') {
             dst[j++] = '\\';
             dst[j++] = 't';
         } else {
             dst[j++] = src[i];
         }
         i++;
     }
     dst[j] = '\0';
 }
 
 /* Отправить HTTP-ответ клиенту */
 static void send_response(int fd, int code, const char *status,
                           const char *content_type, const char *body) {
     char header[512];
     int body_len = body ? (int)strlen(body) : 0;
     snprintf(header, sizeof(header),
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: %s\r\n"
              "Content-Length: %d\r\n"
              "Connection: close\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "\r\n",
              code, status, content_type, body_len);
     write(fd, header, strlen(header));
     if (body && body_len > 0) write(fd, body, body_len);
 }
 
 /* Простой JSON-ответ с одним полем ok/error */
 static void send_json_ok(int fd, const char *json_body) {
     send_response(fd, 200, "OK", "application/json; charset=utf-8", json_body);
 }
 
 static void send_json_error(int fd, int code, const char *msg) {
     char body[256];
     snprintf(body, sizeof(body), "{\"error\":\"%s\"}", msg);
     send_response(fd, code, "Error", "application/json; charset=utf-8", body);
 }
 
 /* Достать значение из строки form: key=value&... */
 static void get_form_value(const char *body, const char *key, char *out, size_t out_size) {
     out[0] = '\0';
     if (!body || !key) return;
 
     char pattern[64];
     snprintf(pattern, sizeof(pattern), "%s=", key);
     const char *p = strstr(body, pattern);
     if (!p) return;
     p += strlen(pattern);
 
     char encoded[2048];
     size_t i = 0;
     while (*p && *p != '&' && i + 1 < sizeof(encoded)) {
         encoded[i++] = *p++;
     }
     encoded[i] = '\0';
     
     url_decode(encoded, out, out_size);
 }
 
 /* Достать token из заголовка Authorization: TOKEN xxx */
 static void get_auth_token(const char *headers, char *token, size_t token_size) {
     token[0] = '\0';
     const char *p = strstr(headers, "Authorization:");
     if (!p) return;
     p += 14;
     while (*p == ' ') p++;
     if (strncmp(p, "TOKEN ", 6) == 0) {
         p += 6;
         size_t i = 0;
         while (*p && *p != '\r' && *p != '\n' && i + 1 < token_size) {
             token[i++] = *p++;
         }
         token[i] = '\0';
     }
 }
 
 /* Отдать статический файл из папки web/ */
 static void serve_static_file(int fd, const char *web_dir, const char *path) {
     char file_path[512];
     const char *name = path;
 
     if (strcmp(path, "/") == 0) name = "/index.html";
 
     snprintf(file_path, sizeof(file_path), "%s%s", web_dir, name);
 
     FILE *f = fopen(file_path, "rb");
     if (!f) {
         send_json_error(fd, 404, "not found");
         return;
     }
 
     const char *ctype = "text/plain";
     if (strstr(name, ".html")) ctype = "text/html; charset=utf-8";
     else if (strstr(name, ".css"))  ctype = "text/css; charset=utf-8";
     else if (strstr(name, ".js"))   ctype = "application/javascript; charset=utf-8";
 
     fseek(f, 0, SEEK_END);
     long size = ftell(f);
     fseek(f, 0, SEEK_SET);
 
     char *buf = malloc(size + 1);
     if (!buf) { fclose(f); return; }
     fread(buf, 1, size, f);
     buf[size] = '\0';
     fclose(f);
 
     send_response(fd, 200, "OK", ctype, buf);
     free(buf);
 }
 
 /* Сформировать JSON со списком пользователей */
 static void build_users_json(const char *self, char *out, size_t out_size) {
     char names[MAX_USERS][MAX_USERNAME];
     int count = 0;
     storage_get_usernames(names, MAX_USERS, &count);
 
     strcpy(out, "{\"users\":[");
     int first = 1;
     for (int i = 0; i < count; i++) {
         if (strcmp(names[i], self) == 0) continue;
         if (!first) strncat(out, ",", out_size - strlen(out) - 1);
         first = 0;
         
         char escaped[MAX_USERNAME * 2];
         json_escape(names[i], escaped, sizeof(escaped));
         
         char item[128];
         snprintf(item, sizeof(item), "\"%s\"", escaped);
         strncat(out, item, out_size - strlen(out) - 1);
     }
     strncat(out, "]}", out_size - strlen(out) - 1);
 }
 
 /* Сформировать JSON с сообщениями */
 static void build_history_json(const char *me, const char *peer, int since, char *out, size_t out_size) {
     Message list[200];
     int count = 0;
     storage_get_messages(me, peer, since, list, 200, &count);
 
     strcpy(out, "{\"messages\":[");
     for (int i = 0; i < count; i++) {
         if (i > 0) strncat(out, ",", out_size - strlen(out) - 1);
         
         char escaped_text[MAX_MSG_TEXT * 2];
         json_escape(list[i].text, escaped_text, sizeof(escaped_text));
         
         char item[2048];
         snprintf(item, sizeof(item),
                  "{\"id\":%d,\"from\":\"%s\",\"to\":\"%s\",\"text\":\"%s\",\"time\":%ld}",
                  list[i].id, list[i].from, list[i].to, escaped_text, list[i].timestamp);
         strncat(out, item, out_size - strlen(out) - 1);
     }
     strncat(out, "]}", out_size - strlen(out) - 1);
 }
 
 /* Обработать один HTTP-запрос */
 static void handle_http_client(int client_fd, const char *web_dir) {
     char buf[8192];
     ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
     if (n <= 0) { close(client_fd); return; }
     buf[n] = '\0';
 
     char method[16] = {0};
     char path[256] = {0};
     sscanf(buf, "%15s %255s", method, path);
 
     char *body = strstr(buf, "\r\n\r\n");
     if (body) body += 4;
 
     char token[MAX_TOKEN];
     get_auth_token(buf, token, sizeof(token));
 
     /* --- API --- */
 
     if (strcmp(method, "POST") == 0 && strcmp(path, "/api/register") == 0) {
         char user[MAX_USERNAME], pass[MAX_PASSWORD];
         get_form_value(body, "username", user, sizeof(user));
         get_form_value(body, "password", pass, sizeof(pass));
         if (storage_register_user(user, pass)) {
             log_event("registered user (web)");
             send_json_ok(client_fd, "{\"ok\":true}");
         } else {
             send_json_error(client_fd, 409, "user exists");
         }
 
     } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/login") == 0) {
         char user[MAX_USERNAME], pass[MAX_PASSWORD];
         get_form_value(body, "username", user, sizeof(user));
         get_form_value(body, "password", pass, sizeof(pass));
         if (storage_check_login(user, pass)) {
             char tok[MAX_TOKEN];
             session_create(user, tok, sizeof(tok));
             log_event("login (web)");
             char json[128];
             snprintf(json, sizeof(json), "{\"token\":\"%s\",\"username\":\"%s\"}", tok, user);
             send_json_ok(client_fd, json);
         } else {
             send_json_error(client_fd, 401, "bad login");
         }
 
     } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/logout") == 0) {
         session_remove(token);
         log_event("logout (web)");
         send_json_ok(client_fd, "{\"ok\":true}");
 
     } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/users") == 0) {
         const char *me = session_find_user(token);
         if (!me) { send_json_error(client_fd, 401, "need login"); close(client_fd); return; }
         char json[2048];
         build_users_json(me, json, sizeof(json));
         send_json_ok(client_fd, json);
 
     } else if (strncmp(method, "GET", 3) == 0 && strncmp(path, "/api/history", 12) == 0) {
         const char *me = session_find_user(token);
         if (!me) { send_json_error(client_fd, 401, "need login"); close(client_fd); return; }
 
         char peer[MAX_USERNAME] = {0};
         int since = 0;
         const char *q = strchr(path, '?');
         if (q) {
             char query[256];
             strncpy(query, q + 1, sizeof(query) - 1);
             query[sizeof(query)-1] = '\0';
             get_form_value(query, "peer", peer, sizeof(peer));
             char since_str[32];
             get_form_value(query, "since", since_str, sizeof(since_str));
             since = atoi(since_str);
         }
 
         char json[16384];
         build_history_json(me, peer, since, json, sizeof(json));
         send_json_ok(client_fd, json);
 
     } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/send") == 0) {
         const char *me = session_find_user(token);
         if (!me) { send_json_error(client_fd, 401, "need login"); close(client_fd); return; }
 
         char to[MAX_USERNAME], text[MAX_MSG_TEXT];
         get_form_value(body, "to", to, sizeof(to));
         get_form_value(body, "text", text, sizeof(text));
 
         int id = storage_add_message(me, to, text);
         if (id > 0) {
             char json[64];
             snprintf(json, sizeof(json), "{\"id\":%d}", id);
             send_json_ok(client_fd, json);
         } else {
             send_json_error(client_fd, 400, "send failed");
         }
 
     } else if (strcmp(method, "GET") == 0) {
         serve_static_file(client_fd, web_dir, path);
 
     } else {
         send_json_error(client_fd, 404, "not found");
     }
 
     close(client_fd);
 }
 
 /* Аргумент для потока: int fd клиента + путь к web */
 typedef struct {
     int fd;
     char web_dir[256];
 } HttpClientArg;
 
 static void *http_client_thread(void *arg) {
     HttpClientArg *a = (HttpClientArg *)arg;
     handle_http_client(a->fd, a->web_dir);
     free(a);
     return NULL;
 }
 
 void http_server_run(int port, const char *web_dir) {
     int server_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (server_fd < 0) {
         perror("http socket");
         return;
     }
 
     int opt = 1;
     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
     struct sockaddr_in addr;
     memset(&addr, 0, sizeof(addr));
     addr.sin_family = AF_INET;
     addr.sin_addr.s_addr = INADDR_ANY;
     addr.sin_port = htons((uint16_t)port);
 
     if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
         perror("http bind");
         close(server_fd);
         return;
     }
 
     if (listen(server_fd, 16) < 0) {
         perror("http listen");
         close(server_fd);
         return;
     }
 
     char msg[128];
     snprintf(msg, sizeof(msg), "HTTP server started on port %d", port);
     log_event(msg);
 
     while (1) {
         struct sockaddr_in client_addr;
         socklen_t len = sizeof(client_addr);
         int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
         if (client_fd < 0) continue;
 
         HttpClientArg *arg = malloc(sizeof(HttpClientArg));
         if (!arg) {
             close(client_fd);
             continue;
         }
         arg->fd = client_fd;
         strncpy(arg->web_dir, web_dir, sizeof(arg->web_dir) - 1);
         arg->web_dir[sizeof(arg->web_dir)-1] = '\0';
 
         pthread_t tid;
         pthread_create(&tid, NULL, http_client_thread, arg);
         pthread_detach(tid);
     }
 }