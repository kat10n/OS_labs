#ifndef UTILS_H
#define UTILS_H

#include <semaphore.h>
#include <stddef.h>

enum Division {
    DIV_OK,
    DIV_ZERO,
    DIV_ERROR
};

/* После заголовка в том же файле лежит строка, включая завершающий нуль. */
typedef struct {
    sem_t request_ready;
    sem_t reply_ready;
    size_t line_size;          /* 0 означает конец ввода */
    enum Division reply;
} Shared;

void die(const char *msg);
enum Division divide_line(const char *s, int *result);

#endif
