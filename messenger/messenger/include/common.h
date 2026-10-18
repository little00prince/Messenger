#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/* Максимальные размеры строк (можно менять при необходимости) */
#define MAX_USERNAME   32
#define MAX_PASSWORD   64
#define MAX_MSG_TEXT   512
#define MAX_LINE       1024
#define MAX_USERS      64
#define MAX_MESSAGES   2000
#define MAX_SESSIONS   64
#define MAX_TOKEN      33   /* 32 символа + '\0' */

/* TCP-порт для сетевого режима и веб-интерфейса */
#define DEFAULT_HTTP_PORT   80

#endif /* COMMON_H */
