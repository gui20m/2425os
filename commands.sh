#!/bin/bash

./bin/dclient -c 1
./bin/dclient -l 1 "Romeo"
./bin/dclient -d 1
./bin/dclient -l 1 "Romeo"
./bin/dclient -s "praia"
./bin/dclient -s "praia" 5
./bin/dclient -d 2
./bin/dclient -s "praia" 10
./bin/dclient -f