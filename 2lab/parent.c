#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"

int main(void)
{
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int p1[2], p2[2];

    signal(SIGPIPE, SIG_IGN);

    printf("[parent %d] имя файла: ", getpid()); 
    fflush(stdout); // вместо "\n" чтобы не было буферизации
    if (getline(&line, &cap, stdin) <= 1) {
        fprintf(stderr, "имя файла не введено\n");
        return EXIT_FAILURE;
    }
    line[strcspn(line, "\n")] = '\0'; // удаляем символ новой строки

    if (pipe(p1) == -1 || pipe(p2) == -1)
        die("pipe");
    pid_t pid = fork(); // создаем дочерний процесс
    if (pid == -1)
        die("fork");

    if (pid == 0) {
        if (dup2(p1[0], STDIN_FILENO) == -1 || dup2(p2[1], STDOUT_FILENO) == -1) // перенаправляем stdin и stdout
            die("dup2");
        close(p1[0]); close(p1[1]); close(p2[0]); close(p2[1]);
        execl("./child", "child", line, (char *)NULL);
        die("execl");
    }

    close(p1[0]);
    close(p2[1]);
    printf("[parent %d] запущен child %d\n", getpid(), pid);

    char resp;
    while ((len = getline(&line, &cap, stdin)) != -1) {
        if (write(p1[1], line, len) != len ||
            (line[len - 1] != '\n' && write(p1[1], "\n", 1) != 1)) {
            perror("write");
            break;
        }
        ssize_t n = read(p2[0], &resp, 1);
        if (n == -1)
            die("read");
        if (n == 0) { 
            fprintf(stderr, "child неожиданно завершился\n");
            break;
        }
        if (resp == RESP_DIV0) {
            printf("[parent %d] деление на 0, выход\n", getpid());
            break;
        }
        printf(resp == RESP_OK ? "ok\n" : "некорректная строка\n");
    }

    close(p1[1]);
    close(p2[0]);
    free(line);

    int status;
    if (waitpid(pid, &status, 0) == -1) // waitpid ждет завершения дочернего процесса и возвращает его статус
        die("waitpid");
    printf("[parent %d] child %d завершился, код %d\n", getpid(), pid, WEXITSTATUS(status));
    return EXIT_SUCCESS;
}