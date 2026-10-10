#!/bin/bash
# Демонстрация числа потоков средствами ОС:
# запускаем сортировку в фоне и несколько раз смотрим
# поле Threads в /proc/PID/status и вывод ps -L (по строке на поток).
P=${1:-4}
make -s || exit 1
./msort -t "$P" -n 100000000 &
PID=$!
sleep 1.5
for i in 1 2 3; do
    grep Threads /proc/$PID/status 2>/dev/null
    sleep 0.3
done
echo "--- ps -L -p $PID ---"
ps -L -o pid,lwp,nlwp,pcpu,comm -p $PID
wait $PID
