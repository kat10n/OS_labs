#define _GNU_SOURCE
#include "msort.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Куски меньше этого размера сортируем в текущем потоке:
 * создание потока дороже, чем сортировка маленького куска. */
#define PAR_THRESHOLD 8192
/* Совсем маленькие куски сортируем вставками. */
#define INSERTION_THRESHOLD 32

/* Аргумент для нового потока: какой диапазон [lo, hi) ему сортировать. */
typedef struct {
    int *arr;
    int *tmp;
    size_t lo;
    size_t hi;
} task_t;

/* Общий счётчик активных потоков, защищён мьютексом. */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_active;   /* сколько потоков сейчас работает (с главным) */
static int g_max;      /* ограничение из ключа -t */
static int g_peak;     /* максимум, которого достиг g_active */
static int g_verbose;

static void lock_or_die(void)
{
    int rc = pthread_mutex_lock(&g_lock);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

static void unlock_or_die(void)
{
    int rc = pthread_mutex_unlock(&g_lock);
    if (rc != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

/* Попытаться занять "слот" под новый поток. 1 - получилось, 0 - лимит. */
static int try_acquire_slot(void)
{
    int ok = 0;
    lock_or_die();
    if (g_active < g_max) {
        g_active++;
        if (g_active > g_peak)
            g_peak = g_active;
        ok = 1;
    }
    unlock_or_die();
    return ok;
}

static void release_slot(void)
{
    lock_or_die();
    g_active--;
    unlock_or_die();
}

static void insertion_sort(int *a, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        int x = a[i];
        size_t j = i;
        while (j > 0 && a[j - 1] > x) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = x;
    }
}

/* Слить отсортированные [lo, mid) и [mid, hi) через буфер tmp. */
static void merge(int *arr, int *tmp, size_t lo, size_t mid, size_t hi)
{
    if (arr[mid - 1] <= arr[mid])   /* уже упорядочено */
        return;

    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi)
        tmp[k++] = (arr[i] <= arr[j]) ? arr[i++] : arr[j++];
    while (i < mid)
        tmp[k++] = arr[i++];
    while (j < hi)
        tmp[k++] = arr[j++];

    memcpy(arr + lo, tmp + lo, (hi - lo) * sizeof(int));
}

static void sort_range(int *arr, int *tmp, size_t lo, size_t hi);

/* Точка входа нового потока. */
static void *worker(void *p)
{
    task_t *t = p;

    if (g_verbose)
        fprintf(stderr, "[PID %d TID %d] старт: сортирую [%zu, %zu)\n",
                getpid(), gettid(), t->lo, t->hi);

    sort_range(t->arr, t->tmp, t->lo, t->hi);

    if (g_verbose)
        fprintf(stderr, "[PID %d TID %d] готово: [%zu, %zu)\n",
                getpid(), gettid(), t->lo, t->hi);
    return NULL;
}

/*
 * Рекурсивная сортировка [lo, hi).
 * Левую половину отдаём новому потоку (если есть свободный слот),
 * правую сортируем сами, потом ждём поток (join) и сливаем половины.
 * Половины не пересекаются, поэтому потоки пишут в разные участки
 * arr и tmp и гонки данных нет.
 */
static void sort_range(int *arr, int *tmp, size_t lo, size_t hi)
{
    size_t n = hi - lo;
    if (n <= INSERTION_THRESHOLD) {
        insertion_sort(arr + lo, n);
        return;
    }

    size_t mid = lo + n / 2;
    pthread_t th;
    task_t left = { arr, tmp, lo, mid };   /* живёт до join, поэтому на стеке */
    int spawned = 0;

    if (n >= PAR_THRESHOLD && try_acquire_slot()) {
        int rc = pthread_create(&th, NULL, worker, &left);
        if (rc == 0) {
            spawned = 1;
        } else {
            /* Не смогли создать поток - не падаем, сортируем сами. */
            fprintf(stderr, "pthread_create: %s (продолжаю в текущем потоке)\n",
                    strerror(rc));
            release_slot();
        }
    }

    if (!spawned)
        sort_range(arr, tmp, lo, mid);
    sort_range(arr, tmp, mid, hi);

    if (spawned) {
        int rc = pthread_join(th, NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }
        /* Слот освобождаем только после join: поток точно завершён,
         * поэтому живых потоков никогда не больше max_threads. */
        release_slot();
    }

    merge(arr, tmp, lo, mid, hi);
}

int parallel_merge_sort(int *arr, size_t n, int max_threads, int verbose)
{
    g_peak = 1;
    if (n < 2)
        return 0;

    int *tmp = malloc(n * sizeof(int));
    if (tmp == NULL) {
        perror("malloc (буфер слияния)");
        return -1;
    }

    g_max = max_threads;
    g_active = 1;          /* главный поток уже работает */
    g_verbose = verbose;

    sort_range(arr, tmp, 0, n);

    free(tmp);
    return 0;
}

int msort_peak_threads(void)
{
    return g_peak;
}
