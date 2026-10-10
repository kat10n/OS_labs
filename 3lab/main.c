#define _GNU_SOURCE
#include "msort.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr,
            "Использование: %s -t ПОТОКИ [-n РАЗМЕР] [-i ФАЙЛ] [-s SEED] [-p] [-v]\n"
            "  -t N     максимум потоков одновременно (обязательный, >= 1)\n"
            "  -n N     размер случайного массива (по умолчанию 1000000)\n"
            "  -i ФАЙЛ  читать целые числа из файла вместо генерации\n"
            "  -s SEED  зерно генератора случайных чисел\n"
            "  -p       напечатать отсортированный массив\n"
            "  -v       печатать PID/TID потоков\n",
            prog);
}

/* Разбор положительного целого из строки; -1 при ошибке. */
static long parse_positive(const char *s)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v <= 0)
        return -1;
    return v;
}

static int *generate(size_t n, unsigned seed)
{
    int *a = malloc(n * sizeof(int));
    if (a == NULL) {
        perror("malloc (массив)");
        return NULL;
    }
    srand(seed);
    for (size_t i = 0; i < n; i++)
        a[i] = rand() - RAND_MAX / 2;
    return a;
}

/* Чтение целых из файла; массив растёт удвоением. */
static int *read_file(const char *path, size_t *out_n)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        perror(path);
        return NULL;
    }

    size_t cap = 1024, n = 0;
    int *a = malloc(cap * sizeof(int));
    if (a == NULL) {
        perror("malloc (массив)");
        fclose(f);
        return NULL;
    }

    int x;
    while (fscanf(f, "%d", &x) == 1) {
        if (n == cap) {
            cap *= 2;
            int *b = realloc(a, cap * sizeof(int));
            if (b == NULL) {
                perror("realloc (массив)");
                free(a);
                fclose(f);
                return NULL;
            }
            a = b;
        }
        a[n++] = x;
    }

    if (ferror(f) || !feof(f)) {
        fprintf(stderr, "%s: ошибка чтения или не число в файле\n", path);
        free(a);
        fclose(f);
        return NULL;
    }
    if (fclose(f) != 0)
        perror("fclose");

    *out_n = n;
    return a;
}

static int is_sorted(const int *a, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (a[i - 1] > a[i])
            return 0;
    return 1;
}

static double now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char *argv[])
{
    long threads = -1;
    long size = 1000000;
    unsigned seed = (unsigned)time(NULL);
    const char *input = NULL;
    int print = 0, verbose = 0;
    int opt;

    while ((opt = getopt(argc, argv, "t:n:i:s:pvh")) != -1) {
        switch (opt) {
        case 't':
            threads = parse_positive(optarg);
            if (threads < 0) {
                fprintf(stderr, "Неверное число потоков: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'n':
            size = parse_positive(optarg);
            if (size < 0) {
                fprintf(stderr, "Неверный размер массива: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'i': input = optarg; break;
        case 's': seed = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'p': print = 1; break;
        case 'v': verbose = 1; break;
        default:
            usage(argv[0]);
            return opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }
    if (threads < 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    size_t n;
    int *arr;
    if (input != NULL) {
        arr = read_file(input, &n);
    } else {
        n = (size_t)size;
        arr = generate(n, seed);
    }
    if (arr == NULL)
        return EXIT_FAILURE;

    printf("PID %d (главный TID %d), элементов: %zu, лимит потоков: %ld\n",
           getpid(), gettid(), n, threads);

    double t0 = now_ms();
    if (parallel_merge_sort(arr, n, (int)threads, verbose) != 0) {
        free(arr);
        return EXIT_FAILURE;
    }
    double t1 = now_ms();

    if (!is_sorted(arr, n)) {
        fprintf(stderr, "ОШИБКА: массив не отсортирован!\n");
        free(arr);
        return EXIT_FAILURE;
    }

    if (print) {
        for (size_t i = 0; i < n; i++)
            printf("%d%c", arr[i], i + 1 == n ? '\n' : ' ');
    }

    printf("Максимум потоков одновременно: %d\n", msort_peak_threads());
    printf("Время сортировки: %.3f мс\n", t1 - t0);

    free(arr);
    return EXIT_SUCCESS;
}
