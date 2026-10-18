#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD]; /* для учебного проекта храним как есть */
} User;

typedef struct {
    int id;
    char from[MAX_USERNAME];
    char to[MAX_USERNAME];
    char text[MAX_MSG_TEXT];
    long timestamp;
} Message;

/* Инициализация и загрузка данных с диска */
void storage_init(const char *data_dir);

/* Пользователи */
int  storage_register_user(const char *username, const char *password);
int  storage_check_login(const char *username, const char *password);
int  storage_user_exists(const char *username);
int  storage_get_user_count(void);
void storage_get_usernames(char names[][MAX_USERNAME], int max_count, int *count);

/* Сообщения (личная переписка между двумя пользователями) */
int  storage_add_message(const char *from, const char *to, const char *text);
int  storage_get_messages(const char *user1, const char *user2, int since_id,
                          Message *out, int max_count, int *out_count);

#endif /* STORAGE_H */
