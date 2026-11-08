#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int report_error(const char *msg)
{
    perror(msg);
    return -1;
}

static int reserve_space(int fd, off_t size)
{
    int error = posix_fallocate(fd, 0, size);
    if (error != 0) {
        errno = error;
        return report_error("parent: posix_fallocate");
    }
    return 0;
}

static int wait_reply(Shared *shared, pid_t child)
{
    for (;;) {
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1)
            return report_error("parent: clock_gettime");
        deadline.tv_sec++;

        if (sem_timedwait(&shared->reply_ready, &deadline) == 0)
            return 0;
        if (errno == EINTR)
            continue;
        if (errno != ETIMEDOUT)
            return report_error("parent: sem_timedwait");

        /* Проверяем завершение, сохраняя статус для финального waitpid. */
        siginfo_t info = {0};
        if (waitid(P_PID, (id_t)child, &info, WEXITED | WNOHANG | WNOWAIT) == -1) {
            if (errno == EINTR)
                continue;
            return report_error("parent: waitid");
        }
        if (info.si_pid != 0) {
            /* Child мог отправить ответ сразу после истечения тайм-аута. */
            if (sem_trywait(&shared->reply_ready) == 0)
                return 0;
            if (errno != EAGAIN)
                return report_error("parent: sem_trywait");
            fprintf(stderr, "parent: child завершился до ответа\n");
            return -1;
        }
    }
}

static int send_line(Shared *shared, int fd, const char *line, size_t line_size)
{
    if (line_size > SIZE_MAX - sizeof(*shared) ||
        line_size > INT64_MAX - sizeof(*shared)) {
        errno = EOVERFLOW;
        return report_error("parent: длина строки");
    }
    size_t size = sizeof(*shared) + line_size;
    if (reserve_space(fd, (off_t)size) == -1)
        return -1;

    char *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
        return report_error("parent: mmap строки");
    memcpy(data + sizeof(*shared), line, line_size);
    if (munmap(data, size) == -1)
        return report_error("parent: munmap строки");

    shared->line_size = line_size;
    if (sem_post(&shared->request_ready) == -1)
        return report_error("parent: sem_post");
    return 0;
}

static int exchange(Shared *shared, int fd, pid_t child, char **line, size_t *cap)
{
    if (wait_reply(shared, child) == -1)
        return -1;
    printf("[parent %d] запущен child %d\n", getpid(), child);

    ssize_t len;
    while ((len = getline(line, cap, stdin)) != -1) {
        if (send_line(shared, fd, *line, (size_t)len + 1) == -1 ||
            wait_reply(shared, child) == -1)
            return -1;

        if (shared->reply == DIV_ZERO) {
            printf("[parent %d] деление на 0, выход\n", getpid());
            return 0;
        }
        puts(shared->reply == DIV_OK ? "ok" : "некорректная строка");
    }

    if (!feof(stdin))
        return report_error("parent: getline");
    shared->line_size = 0;
    if (sem_post(&shared->request_ready) == -1)
        return report_error("parent: sem_post");
    return 0;
}

static int finish_child(pid_t child)
{
    int status;
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR)
            return report_error("parent: waitpid");
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        printf("[parent %d] child %d завершился, код %d\n", getpid(), child, code);
        return code == 0 ? 0 : -1;
    }
    if (WIFSIGNALED(status))
        fprintf(stderr, "[parent %d] child %d завершён сигналом %d\n",
                getpid(), child, WTERMSIG(status));
    return -1;
}

int main(void)
{
    char *line = NULL;
    size_t cap = 0;

    printf("[parent %d] имя файла: ", getpid());
    if (fflush(stdout) == EOF)
        die("parent: fflush");

    ssize_t len = getline(&line, &cap, stdin);
    if (len == -1 && !feof(stdin))
        die("parent: getline");
    if (len > 0 && line[len - 1] == '\n')
        line[--len] = '\0';
    if (len <= 0 || memchr(line, '\0', (size_t)len) != NULL) {
        fprintf(stderr, "parent: некорректное имя файла\n");
        free(line);
        return EXIT_FAILURE;
    }

    char path[] = "/tmp/lab4-XXXXXX";
    int fd = mkstemp(path);
    if (fd == -1)
        die("parent: mkstemp");
    if (unlink(path) == -1)
        die("parent: unlink");
    if (reserve_space(fd, sizeof(Shared)) == -1)
        return EXIT_FAILURE;

    Shared *shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED)
        die("parent: mmap заголовка");
    if (sem_init(&shared->request_ready, 1, 0) == -1)
        die("parent: sem_init request_ready");
    if (sem_init(&shared->reply_ready, 1, 0) == -1)
        die("parent: sem_init reply_ready");

    char fd_arg[32];
    snprintf(fd_arg, sizeof(fd_arg), "%d", fd);
    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR)
        die("parent: signal SIGCHLD");
    pid_t parent = getpid();
    pid_t child = fork();
    if (child == -1)
        die("parent: fork");
    if (child == 0) {
        /* Если parent аварийно завершится, child не останется ждать запроса. */
        if (prctl(PR_SET_PDEATHSIG, (unsigned long)SIGKILL, 0UL, 0UL, 0UL) == -1) {
            perror("child: prctl");
            _exit(EXIT_FAILURE);
        }
        if (getppid() != parent)
            _exit(EXIT_FAILURE);
        execl("./child", "child", line, fd_arg, (char *)NULL);
        perror("parent: execl ./child");
        _exit(EXIT_FAILURE);
    }

    int result = exchange(shared, fd, child, &line, &cap);
    if (result == -1 && kill(child, SIGKILL) == -1 && errno != ESRCH)
        perror("parent: kill");
    if (finish_child(child) == -1)
        result = -1;

    free(line);
    if (sem_destroy(&shared->request_ready) == -1)
        result = report_error("parent: sem_destroy request_ready");
    if (sem_destroy(&shared->reply_ready) == -1)
        result = report_error("parent: sem_destroy reply_ready");
    if (munmap(shared, sizeof(*shared)) == -1)
        result = report_error("parent: munmap заголовка");
    if (close(fd) == -1)
        result = report_error("parent: close");

    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
