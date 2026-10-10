#ifndef UTILS_H
#define UTILS_H

#define RESP_OK    'O'
#define RESP_ERROR 'E'
#define RESP_DIV0  'Z'

#define DIV_OK    0
#define DIV_ZERO  1
#define DIV_ERROR 2

void die(const char *msg);                    /* perror + exit(1) */
int divide_line(const char *s, int *res);     /* первое число делит на остальные */

#endif