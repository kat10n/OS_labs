#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static int in_int(long v)
{
    return errno != ERANGE && v >= INT_MIN && v <= INT_MAX;
}

int divide_line(const char *s, int *res)
{
    char *end;
    errno = 0;
    long cur = strtol(s, &end, 10);
    if (end == s || !in_int(cur))
        return DIV_ERROR;

    for (s = end; ; s = end) {
        errno = 0;
        long d = strtol(s, &end, 10);
        if (end == s)
            break;
        if (!in_int(d))
            return DIV_ERROR;
        if (d == 0)
            return DIV_ZERO;
        cur /= d;
    }

    while (isspace((unsigned char)*s))
        s++;
    if (*s != '\0' || !in_int(cur))
        return DIV_ERROR;
    *res = (int)cur;
    return DIV_OK;
}