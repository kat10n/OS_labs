#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static int in_int(int64_t value)
{
    return value >= INT_MIN && value <= INT_MAX;
}

enum Division divide_line(const char *s, int *result)
{
    char *end;
    errno = 0;
    int64_t current = strtol(s, &end, 10);
    if (end == s || errno == ERANGE || !in_int(current))
        return DIV_ERROR;

    while (*end != '\0') {
        if (!isspace((unsigned char)*end))
            return DIV_ERROR;

        s = end;
        errno = 0;
        long divisor = strtol(s, &end, 10);
        if (end == s) {
            while (isspace((unsigned char)*s))
                s++;
            if (*s != '\0')
                return DIV_ERROR;
            break;
        }
        if (errno == ERANGE || !in_int(divisor) ||
            (*end != '\0' && !isspace((unsigned char)*end)))
            return DIV_ERROR;
        if (divisor == 0)
            return DIV_ZERO;

        /* Широкий тип позволяет безопасно вычислить INT_MIN / -1. */
        current /= divisor;
    }

    if (!in_int(current))
        return DIV_ERROR;
    *result = (int)current;
    return DIV_OK;
}
