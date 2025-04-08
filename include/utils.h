#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

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
    int id;
    char title[200];
    char authors[200];
    int year;
    char path[64];
    int valid; // flag para saber se um documento foi apagado
    int used; // numero de acessos ao documento
    time_t last_access; // timestamp da ultima vez que o ficheiro foi acedido
} Document;

void createFIFO(char* fifo_name);

char* remove_document(int taskid, int id, Document docs[], int *total, int available_indexs[]);

int is_valid_document(char* doc_folder, char* rel_path);

int count_line_w_keyword(char* path, char* keyword);

char* match_pattern(Document documents[], int *total_documents, char* keyword, char* route, int cache_size, int num_processes);

char* remove_duplicates(char* str);

void save_metadata(char* filename, Document* documents, int *total_documents, int *valid_index);

int load_metadata(char* filename, Document docs[], int max_size, int *loaded, int* total_docs);

void update_metadata(char* filename, int document_id);

int try_insert(Task task, Document documents[], int available_indexs[], int cache_size, int* total_documents);

int cache_files(Task task, Document* documents, int* total_documents, int cache_size);

int load_from_metadata(char* filename, Document documents[], int id, int cache_size, int* total_docs);

int least_used_frequently(Document documents[], int cache_size);

#endif