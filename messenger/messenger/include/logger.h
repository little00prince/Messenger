#ifndef LOGGER_H
#define LOGGER_H

/* Инициализация: создаёт папку logs внутри data_dir */
void logger_init(const char *data_dir);

/* Запись в server.log */
void log_info(const char *text);
void log_event(const char *text);

/* Запись сообщений в messages.log */
void log_message_sent(const char *from, const char *to, const char *text);
void log_message_recv(const char *from, const char *to, const char *text);

#endif /* LOGGER_H */
