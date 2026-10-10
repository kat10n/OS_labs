#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"


int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: child <file>\n");
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
        die("child: open");
    fprintf(stderr, "[child %d] старт, parent %d\n", getpid(), getppid());

    char *line = NULL;
    size_t cap = 0;
    int res;
    while (getline(&line, &cap, stdin) != -1) {     /* -1 = EOF, parent закрыл pipe1 */
        int r = divide_line(line, &res);
        if (r == DIV_OK && dprintf(fd, "%d\n", res) < 0)
            die("child: dprintf");
        char resp = (r == DIV_OK) ? RESP_OK : (r == DIV_ZERO) ? RESP_DIV0 : RESP_ERROR;
        if (write(STDOUT_FILENO, &resp, 1) != 1)
            die("child: write");
        if (r == DIV_ZERO)
            break;
    }
    free(line);
    if (close(fd) == -1)
        die("child: close");
    fprintf(stderr, "[child %d] выход\n", getpid());
    return EXIT_SUCCESS;
}