#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static enum Division read_line(int fd, size_t line_size, int *result)
{
    size_t size = sizeof(Shared) + line_size;
    char *data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
        die("child: mmap строки");

    const char *line = data + sizeof(Shared);
    enum Division reply = DIV_ERROR;
    if (memchr(line, '\0', line_size) == line + line_size - 1)
        reply = divide_line(line, result);
    if (munmap(data, size) == -1)
        die("child: munmap строки");
    return reply;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: child <file> <mapping-fd>\n");
        return EXIT_FAILURE;
    }
    char *end;
    errno = 0;
    long descriptor = strtol(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || errno == ERANGE ||
        descriptor < 0 || descriptor > INT_MAX) {
        fprintf(stderr, "child: некорректный дескриптор\n");
        return EXIT_FAILURE;
    }
    int fd = (int)descriptor;

    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1)
        die("child: fstat");
    if (!S_ISREG(statbuf.st_mode) || statbuf.st_size < (off_t)sizeof(Shared)) {
        fprintf(stderr, "child: некорректный файл обмена\n");
        return EXIT_FAILURE;
    }

    Shared *shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED)
        die("child: mmap заголовка");
    int out = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out == -1)
        die("child: open");

    fprintf(stderr, "[child %d] старт, parent %d\n", getpid(), getppid());
    if (sem_post(&shared->reply_ready) == -1)
        die("child: sem_post готовности");

    for (;;) {
        while (sem_wait(&shared->request_ready) == -1) {
            if (errno != EINTR)
                die("child: sem_wait");
        }
        if (shared->line_size == 0)
            break;

        int result;
        enum Division reply = read_line(fd, shared->line_size, &result);
        if (reply == DIV_OK && dprintf(out, "%d\n", result) < 0)
            die("child: запись результата");
        shared->reply = reply;
        if (sem_post(&shared->reply_ready) == -1)
            die("child: sem_post ответа");
        if (reply == DIV_ZERO)
            break;
    }

    if (close(out) == -1)
        die("child: close результата");
    if (munmap(shared, sizeof(*shared)) == -1)
        die("child: munmap заголовка");
    if (close(fd) == -1)
        die("child: close файла обмена");
    fprintf(stderr, "[child %d] выход\n", getpid());
    return EXIT_SUCCESS;
}
