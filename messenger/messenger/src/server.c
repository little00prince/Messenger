 #include "common.h"
 #include "logger.h"
 #include "storage.h"
 #include "http_server.h"
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <signal.h>
 
 static volatile int running = 1;
 
 /* Обработка Ctrl+C */
 static void on_signal(int sig) {
     (void)sig;
     running = 0;
     log_event("server stopping...");
 }
 
 static void print_usage(const char *prog) {
     printf("Использование: %s [опции]\n", prog);
     printf("  --data DIR     каталог данных (по умолчанию ./data)\n");
     printf("  --web DIR      каталог веб-файлов (по умолчанию ./web)\n");
     printf("  --http PORT    порт HTTP (по умолчанию %d)\n", DEFAULT_HTTP_PORT);
     printf("  --help         показать справку\n");
 }
 
 int main(int argc, char *argv[]) {
     const char *data_dir = "./data";
     const char *web_dir  = "./web";
     int http_port = DEFAULT_HTTP_PORT;
 
     for (int i = 1; i < argc; i++) {
         if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) data_dir = argv[++i];
         else if (strcmp(argv[i], "--web") == 0 && i + 1 < argc) web_dir = argv[++i];
         else if (strcmp(argv[i], "--http") == 0 && i + 1 < argc) http_port = atoi(argv[++i]);
         else if (strcmp(argv[i], "--help") == 0) { print_usage(argv[0]); return 0; }
         else { print_usage(argv[0]); return 1; }
     }
 
     signal(SIGINT, on_signal);
     signal(SIGTERM, on_signal);
 
     logger_init(data_dir);
     storage_init(data_dir);
     log_event("server started");
 
     /* HTTP сервер работает в главном потоке */
     http_server_run(http_port, web_dir);
 
     log_event("server stopped");
     return 0;
 }