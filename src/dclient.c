#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"

#include "utils.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        char* buf = "Usage:\n"
                    "  index file: ./dclient -a \"title\" \"authors\" \"year\" \"path\"\n"
                    "  consult meta-data: ./dclient -c \"id\"\n";
        write(STDOUT_FILENO, buf, strlen(buf));
        return 1;
    }

    int pid = getpid();
    char fifo_name[50];
    sprintf(fifo_name, "tmp/client_%d", pid);
    createFIFO(fifo_name);

    int server_fifo = open(SERVER, O_WRONLY);
    if (server_fifo == -1) {
        perror("Error opening server FIFO");
        return 1;
    }

    if (strcmp(argv[1], "-a") == 0 && argc == 6) {
        Task task;
        task.type = 'a';
        strncpy(task.title, argv[2], sizeof(task.title) - 1);
        task.title[sizeof(task.title) - 1] = '\0';
        strncpy(task.authors, argv[3], sizeof(task.authors) - 1);
        task.authors[sizeof(task.authors) - 1] = '\0';
        task.year = atoi(argv[4]);
        strncpy(task.path, argv[5], sizeof(task.path) - 1);
        task.path[sizeof(task.path) - 1] = '\0';
        strncpy(task.client_fifo, fifo_name, sizeof(task.client_fifo) - 1);

        write(server_fifo, &task, sizeof(Task));
        close(server_fifo);

        int client_fifo = open(fifo_name, O_RDONLY);
        if (client_fifo == -1) {
            perror("Error opening client FIFO");
            return 1;
        }

        char response[128];
        int read_bytes = read(client_fifo, response, 128);
        if (read_bytes > 0) {
            write(STDOUT_FILENO, response, read_bytes);
        }

        close(client_fifo);
    }
    if (strcmp(argv[1], "-c") == 0 && argc == 3) {
        Task task;
        task.type = 'c';
        task.id = atoi(argv[2]);
        strncpy(task.client_fifo, fifo_name, sizeof(task.client_fifo) - 1);

        write(server_fifo, &task, sizeof(Task));
        close(server_fifo);

        int client_fifo = open(fifo_name, O_RDONLY);
        if (client_fifo == -1) {
            perror("Error opening client FIFO");
            return 1;
        }

        char response[128];
        int read_bytes;
        while ((read_bytes = read(client_fifo, response, sizeof(response))) > 0) {
            write(STDOUT_FILENO, response, read_bytes);
        }

        close(client_fifo);
    } 
    if (strcmp(argv[1], "-d") == 0 && argc == 3) {
        Task task;
        task.type = 'd';
        task.id = atoi(argv[2]);
        strncpy(task.client_fifo, fifo_name, sizeof(task.client_fifo) - 1);

        write(server_fifo, &task, sizeof(Task));
        close(server_fifo);

        int client_fifo = open(fifo_name, O_RDONLY);
        if (client_fifo == -1) {
            perror("Error opening client FIFO");
            return 1;
        }

        char response[128];
        int read_bytes = read(client_fifo, response, sizeof(response));
        if (read_bytes > 0) {
            write(STDOUT_FILENO, response, read_bytes);
        }

        close(client_fifo);
    }

    if (strcmp(argv[1], "-l") == 0 && argc == 4) {
        Task task;
        task.type = 'l';
        task.id = atoi(argv[2]);
        strncpy(task.keyword, argv[3], sizeof(task.keyword)-1);
        strncpy(task.client_fifo, fifo_name, sizeof(task.client_fifo) - 1);

        write(server_fifo, &task, sizeof(Task));
        close(server_fifo);

        int client_fifo = open(fifo_name, O_RDONLY);
        if (client_fifo == -1) {
            perror("Error opening client FIFO");
            return 1;
        }

        char response[128];
        int read_bytes = read(client_fifo, response, sizeof(response));
        if (read_bytes > 0) {
            write(STDOUT_FILENO, response, read_bytes);
        }

        close(client_fifo);
    }

    if (strcmp(argv[1], "-f") == 0 && argc == 2) {
        Task task;
        task.type = 'f';

        strncpy(task.client_fifo, fifo_name, sizeof(task.client_fifo) - 1);

        write(server_fifo, &task, sizeof(Task));
        close(server_fifo);

        int client_fifo = open(fifo_name, O_RDONLY);
        if (client_fifo == -1) {
            perror("Error opening client FIFO");
            return 1;
        }

        char response[128];
        int read_bytes = read(client_fifo, response, sizeof(response));
        if (read_bytes > 0) {
            write(STDOUT_FILENO, response, read_bytes);
        }

        close(client_fifo);
    }

    unlink(fifo_name);
    return 0;
}