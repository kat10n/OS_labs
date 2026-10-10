#!/bin/bash
# Замер ускорения S = T1/Tp и эффективности E = S/p.
# Для каждой пары (n, p) берём лучшее время из RUNS запусков.
# Результат: таблица в терминале и results.csv

export LC_NUMERIC=C
RUNS=3
SIZES="100000 1000000 10000000 50000000"
THREADS="1 2 4 6 8 12 16 24"

make -s || exit 1
echo "n;p;time_ms;speedup;efficiency" > results.csv

for n in $SIZES; do
    printf "\nn = %d\n%4s %12s %9s %9s\n" "$n" "p" "T, мс" "S" "E"
    t1=""
    for p in $THREADS; do
        best=""
        for r in $(seq $RUNS); do
            t=$(./msort -t "$p" -n "$n" -s 42 | awk '/Время/ {print $3}')
            [ -z "$t" ] && { echo "ошибка запуска"; exit 1; }
            if [ -z "$best" ] || awk "BEGIN{exit !($t < $best)}"; then best=$t; fi
        done
        [ -z "$t1" ] && t1=$best
        s=$(awk "BEGIN{printf \"%.2f\", $t1/$best}")
        e=$(awk "BEGIN{printf \"%.2f\", $t1/$best/$p}")
        printf "%4d %12.2f %9s %9s\n" "$p" "$best" "$s" "$e"
        echo "$n;$p;$best;$s;$e" >> results.csv
    done
done
