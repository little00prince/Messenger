#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

/* Запускает HTTP-сервер (блокирует поток) */
void http_server_run(int port, const char *web_dir);

#endif /* HTTP_SERVER_H */
