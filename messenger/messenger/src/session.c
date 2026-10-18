#include "session.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

typedef struct {
    int used;
    char token[MAX_TOKEN];
    char username[MAX_USERNAME];
} Session;

static Session sessions[MAX_SESSIONS];
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int token_counter = 0;

int session_create(const char *username, char *token_out, size_t token_size) {
    pthread_mutex_lock(&session_mutex);

    /* Ищем свободное место */
    int slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pthread_mutex_unlock(&session_mutex);
        return 0;
    }

    /* Простой token: время + счётчик (для учебного проекта достаточно) */
    token_counter++;
    snprintf(sessions[slot].token, MAX_TOKEN, "tok%lu_%u",
             (unsigned long)time(NULL), token_counter);
    strncpy(sessions[slot].username, username, MAX_USERNAME - 1);
    sessions[slot].used = 1;

    strncpy(token_out, sessions[slot].token, token_size - 1);
    token_out[token_size - 1] = '\0';

    pthread_mutex_unlock(&session_mutex);
    return 1;
}

const char *session_find_user(const char *token) {
    if (!token) return NULL;

    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].used && strcmp(sessions[i].token, token) == 0) {
            const char *name = sessions[i].username;
            pthread_mutex_unlock(&session_mutex);
            return name;
        }
    }
    pthread_mutex_unlock(&session_mutex);
    return NULL;
}

void session_remove(const char *token) {
    if (!token) return;

    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].used && strcmp(sessions[i].token, token) == 0) {
            sessions[i].used = 0;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
}
