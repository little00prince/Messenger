#include "logger.h"

 #include <stdio.h>
 #include <string.h>
 #include <time.h>
 #include <sys/stat.h>
 
 // Увеличиваем размер буферов для путей
 static char server_log_path[1024];
 static char messages_log_path[1024];
 
 /* Создаём каталог, если его ещё нет */
 static void make_dir(const char *path) {
     mkdir(path, 0755);
 }
 
 /* Текущее время в виде строки */
 static void now_string(char *buf, size_t size) {
     time_t t = time(NULL);
     struct tm tm_info;
     localtime_r(&t, &tm_info);
     strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_info);
 }
 
 /* Запись одной строки в указанный файл */
 static void append_line(const char *file, const char *line) {
     FILE *f = fopen(file, "a");
     if (!f) return;
     fprintf(f, "%s\n", line);
     fclose(f);
 }
 
 void logger_init(const char *data_dir) {
     char logs_dir[1024];  // Увеличили размер
     int needed;
     
     // Проверяем длину пути к logs директории
     needed = snprintf(logs_dir, sizeof(logs_dir), "%s/logs", data_dir);
     if (needed >= (int)sizeof(logs_dir)) {
         // Путь слишком длинный, выводим ошибку
         fprintf(stderr, "Error: data_dir path too long: %s\n", data_dir);
         return;
     }
     
     make_dir(data_dir);
     make_dir(logs_dir);
     
     // Проверяем длину путей к лог-файлам
     needed = snprintf(server_log_path, sizeof(server_log_path), "%s/server.log", logs_dir);
     if (needed >= (int)sizeof(server_log_path)) {
         fprintf(stderr, "Error: server log path too long\n");
         return;
     }
     
     needed = snprintf(messages_log_path, sizeof(messages_log_path), "%s/messages.log", logs_dir);
     if (needed >= (int)sizeof(messages_log_path)) {
         fprintf(stderr, "Error: messages log path too long\n");
         return;
     }
 }
 
 void log_info(const char *text) {
     char time_buf[32];
     char line[512];
     now_string(time_buf, sizeof(time_buf));
     snprintf(line, sizeof(line), "[%s] %s", time_buf, text);
     append_line(server_log_path, line);
 }
 
 void log_event(const char *text) {
     log_info(text);
 }
 
 void log_message_sent(const char *from, const char *to, const char *text) {
     char time_buf[32];
     char line[1024];
     now_string(time_buf, sizeof(time_buf));
     snprintf(line, sizeof(line), "[%s] SEND %s -> %s : %s", time_buf, from, to, text);
     append_line(messages_log_path, line);
     log_info(line);
 }
 
 void log_message_recv(const char *from, const char *to, const char *text) {
     char time_buf[32];
     char line[1024];
     now_string(time_buf, sizeof(time_buf));
     snprintf(line, sizeof(line), "[%s] RECV %s -> %s : %s", time_buf, from, to, text);
     append_line(messages_log_path, line);
 }