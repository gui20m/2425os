CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS =

all: folders dserver dclient 

dserver: bin/dserver

dclient: bin/dclient

folders:
	@mkdir -p src include obj bin tmp

bin/dserver: obj/dserver.o obj/utils.o
	@$(CC) $(LDFLAGS) $^ -o $@

bin/dclient: obj/dclient.o obj/utils.o
	@$(CC) $(LDFLAGS) $^ -o $@

bin/utils: obj/utils.o
	@$(CC) $(LDFLAGS) $^ -o $@

obj/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f obj/* tmp/* bin/*