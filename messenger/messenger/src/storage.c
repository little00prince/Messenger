#include "storage.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

static User users[MAX_USERS];
static int user_count = 0;

static Message messages[MAX_MESSAGES];
static int message_count = 0;
static int next_message_id = 1;

static char users_file[256];
static char messages_file[256];
static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

static void make_dir(const char *path) {
    mkdir(path, 0755);
}

/* Загрузка пользователей из файла */
static void load_users(void) {
    FILE *f = fopen(users_file, "r");
    if (!f) return;

    char user[MAX_USERNAME], pass[MAX_PASSWORD];
    while (fscanf(f, "%31s %63s", user, pass) == 2) {
        if (user_count >= MAX_USERS) break;
        snprintf(users[user_count].username, MAX_USERNAME, "%s", user);
        snprintf(users[user_count].password, MAX_PASSWORD, "%s", pass);
        user_count++;
    }
    fclose(f);
}

/* Загрузка сообщений из файла */
static void load_messages(void) {
    FILE *f = fopen(messages_file, "r");
    if (!f) return;

    Message msg;
    while (fscanf(f, "%d %31s %31s %511[^\n]", &msg.id, msg.from, msg.to, msg.text) == 4) {
        if (message_count >= MAX_MESSAGES) break;
        msg.timestamp = 0;
        messages[message_count++] = msg;
        if (msg.id >= next_message_id) next_message_id = msg.id + 1;
    }
    fclose(f);
}

void storage_init(const char *data_dir) {
    make_dir(data_dir);
    snprintf(users_file, sizeof(users_file), "%s/users.txt", data_dir);
    snprintf(messages_file, sizeof(messages_file), "%s/messages.txt", data_dir);
    load_users();
    load_messages();
}

int storage_register_user(const char *username, const char *password) {
    if (!username || !password || username[0] == '\0') return 0;

    pthread_mutex_lock(&storage_mutex);

    /* Проверяем, что такого пользователя ещё нет */
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            pthread_mutex_unlock(&storage_mutex);
            return 0;
        }
    }

    if (user_count >= MAX_USERS) {
        pthread_mutex_unlock(&storage_mutex);
        return 0;
    }

    strncpy(users[user_count].username, username, MAX_USERNAME - 1);
    strncpy(users[user_count].password, password, MAX_PASSWORD - 1);
    user_count++;

    /* Сразу дописываем в файл */
    FILE *f = fopen(users_file, "a");
    if (f) {
        fprintf(f, "%s %s\n", username, password);
        fclose(f);
    }

    pthread_mutex_unlock(&storage_mutex);
    return 1;
}

int storage_check_login(const char *username, const char *password) {
    pthread_mutex_lock(&storage_mutex);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            pthread_mutex_unlock(&storage_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&storage_mutex);
    return 0;
}

int storage_user_exists(const char *username) {
    pthread_mutex_lock(&storage_mutex);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            pthread_mutex_unlock(&storage_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&storage_mutex);
    return 0;
}

int storage_get_user_count(void) {
    return user_count;
}

void storage_get_usernames(char names[][MAX_USERNAME], int max_count, int *count) {
    pthread_mutex_lock(&storage_mutex);
    *count = 0;
    for (int i = 0; i < user_count && *count < max_count; i++) {
        snprintf(names[*count], MAX_USERNAME, "%s", users[i].username);
        (*count)++;
    }
    pthread_mutex_unlock(&storage_mutex);
}

int storage_add_message(const char *from, const char *to, const char *text) {
    if (!storage_user_exists(to)) return 0;

    pthread_mutex_lock(&storage_mutex);
    if (message_count >= MAX_MESSAGES) {
        pthread_mutex_unlock(&storage_mutex);
        return 0;
    }

    Message *m = &messages[message_count];
    m->id = next_message_id++;
    strncpy(m->from, from, MAX_USERNAME - 1);
    strncpy(m->to, to, MAX_USERNAME - 1);
    strncpy(m->text, text, MAX_MSG_TEXT - 1);
    m->timestamp = (long)time(NULL);
    message_count++;

    FILE *f = fopen(messages_file, "a");
    if (f) {
        /* В файле сохраняем id, from, to, text */
        fprintf(f, "%d %s %s %s\n", m->id, m->from, m->to, m->text);
        fclose(f);
    }

    pthread_mutex_unlock(&storage_mutex);

    log_message_sent(from, to, text);
    log_message_recv(from, to, text);
    return m->id;
}

int storage_get_messages(const char *user1, const char *user2, int since_id,
                         Message *out, int max_count, int *out_count) {
    pthread_mutex_lock(&storage_mutex);
    *out_count = 0;

    for (int i = 0; i < message_count && *out_count < max_count; i++) {
        Message *m = &messages[i];
        if (m->id <= since_id) continue;

        int match =
            (strcmp(m->from, user1) == 0 && strcmp(m->to, user2) == 0) ||
            (strcmp(m->from, user2) == 0 && strcmp(m->to, user1) == 0);

        if (match) {
            out[*out_count] = *m;
            (*out_count)++;
        }
    }

    pthread_mutex_unlock(&storage_mutex);
    return 1;
}
