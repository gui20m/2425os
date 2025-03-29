#!/bin/bash

./bin/dserver tmp 1500 &

SERVER_PID=$!

for ((i = 1; i <= 1500; i++))
do
    if (( i == 2 || i == 3 || i == 1438 )); then
        echo "praia" > "tmp/${i}.txt"
    elif (( i == 1 )); then
        for ((j = 0; j < 150; j++))
        do
            echo "Romeo" >> "tmp/${i}.txt"
        done
    else
        echo "" > "tmp/${i}.txt"
    fi
    
    ./bin/dclient -a "Romeo and Juliet" "William Shakespeare" "1997" "${i}.txt"
done

wait $SERVER_PID 2>/dev/null