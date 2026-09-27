/* child: stdin = pipe1 (строки от parent), stdout = pipe2 (ответы parent).
 * Результаты пишет в файл, имя которого получает в argv[1].
 * Диагностика идёт в stderr — он остался терминалом. */
#define _GNU_SOURCE           /* для getline и dprintf */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "utils.h"

/* Отправить parent однобайтовый ответ через stdout (= pipe2) */
static void reply(char code)
{
    if (write_all(STDOUT_FILENO, &code, 1) == -1) {
        perror("child: write to pipe2");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "[child  PID %d, PPID %d] старт, файл: %s\n", getpid(), getppid(), argv[1]);

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("child: open");
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t cap = 0;
    int res;

    /* getline читает строку любой длины из stdin (т.е. из pipe1).
     * Вернёт -1, когда parent закроет свой конец pipe1 (EOF). */
    while (getline(&line, &cap, stdin) != -1) {
        switch (divide_line(line, &res)) {
        case DIV_OK:
            if (dprintf(fd, "%d\n", res) < 0) {
                perror("child: write to file");
                free(line);
                close(fd);
                return EXIT_FAILURE;
            }
            fprintf(stderr, "[child  PID %d] результат %d записан\n", getpid(), res);
            reply(RESP_OK);
            break;
        case DIV_ERROR:
            reply(RESP_ERROR);
            break;
        case DIV_ZERO:
            fprintf(stderr, "[child  PID %d] деление на 0, завершаюсь\n", getpid());
            reply(RESP_DIV0);
            free(line);
            close(fd);
            return CHILD_EXIT_DIV0;
        }
    }

    if (ferror(stdin))
        perror("child: read stdin");

    free(line);
    if (close(fd) == -1) {
        perror("child: close");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "[child  PID %d] stdin закрыт (EOF), выхожу\n", getpid());
    return EXIT_SUCCESS;
}