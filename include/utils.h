#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef UTILS_H
#define UTILS_H

#define SERVER "tmp/server_fifo"
#define META_FILE "tmp/meta.bin"


typedef struct {
    char type;
    int id;
    int nr_processes;
    char keyword[32];
    char title[200];
    char authors[200];
    int year;
    char path[64];
    char client_fifo[47];  // FIFO do cliente para resposta
} Task;

typedef struct {
    char title[200];
    char authors[200];
    int year;
    char path[64];
    int valid;
} Document;

void createFIFO(char* fifo_name);

char* remove_document(int id, Document docs[], int *total);

int is_valid_document(char* doc_folder, char* rel_path);

int count_line_w_keyword(char* path, char* keyword);

char* match_pattern(Document documents[], int *total_documents, char* keyword, char* route);

void save_metadata(const char* filename, Document docs[], int total);

int load_metadata(const char* filename, Document docs[], int max_size, int *loaded);

#endif