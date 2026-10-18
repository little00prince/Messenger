#ifndef SESSION_H
#define SESSION_H

#include "common.h"

/* Создать сессию, вернуть 1 при успехе и записать token */
int session_create(const char *username, char *token_out, size_t token_size);

/* Проверить token, вернуть имя пользователя или NULL */
const char *session_find_user(const char *token);

/* Удалить сессию (выход) */
void session_remove(const char *token);

#endif /* SESSION_H */
