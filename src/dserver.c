#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        char* info = "Usage: <document_folder> <cache_size>\n";
        write(STDOUT_FILENO, info, strlen(info));
        return 1;
    }

    int cache_size = atoi(argv[2]);

    Document documents[cache_size];
    int total_documents = 0;

    if (load_metadata(META_FILE, documents, cache_size, &total_documents) == 0) {
        cache_size = total_documents;
    }
    if (total_documents<=cache_size) printf("[server-log] loaded %d from disk..\n", total_documents);

    int available_indexs[cache_size];
    for (int i=0; i<cache_size; i++) available_indexs[i] =0;

    createFIFO(SERVER);

    int server_fifo = open(SERVER, O_RDONLY);
    if (server_fifo == -1) {
        perror("Error opening server FIFO");
        return 1;
    }

    while (1) {
        Task task;
        int read_bytes = read(server_fifo, &task, sizeof(Task));
        if (read_bytes > 0) {
            if (task.type == 'a' && total_documents>atoi(argv[2])-1) {
                if (!is_valid_document(argv[1], task.path)) {
                    int client_fifo = open(task.client_fifo, O_WRONLY);
                    if (client_fifo != -1) {
                        write(client_fifo, "Error: Invalid document\n", 24);
                        close(client_fifo);
                    }
                    continue;
                }
                int try_id;
                if ((try_id = try_insert(task, documents, available_indexs, cache_size))!=0) {
                    printf("[server-log] document%d: title: %s, author: %s, year: %d, path: %s\n", try_id, task.title, task.authors, task.year, task.path);
                    int client_fifo = open(task.client_fifo, O_WRONLY);
                    if (client_fifo != -1) {
                        char response[128];
                        snprintf(response, sizeof(response), "Document %d indexed\n", try_id);
                        write(client_fifo, response, sizeof(response));
                        close(client_fifo);
                    }
                    continue;
                }
                printf("[server-log] cache is full, cant index more files\n");
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    write(client_fifo, "Error: Cache full\n", 19);
                    close(client_fifo);
                }
                continue;
            }
            if (task.type == 'a') {
                if (!is_valid_document(argv[1], task.path)) {
                    int client_fifo = open(task.client_fifo, O_WRONLY);
                    if (client_fifo != -1) {
                        write(client_fifo, "Error: Invalid document\n", 24);
                        close(client_fifo);
                    }
                    continue;
                }
                strncpy(documents[total_documents].title, task.title, sizeof(documents[total_documents].title));
                strncpy(documents[total_documents].authors, task.authors, sizeof(documents[total_documents].authors));
                documents[total_documents].year = task.year;
                strncpy(documents[total_documents].path, task.path, sizeof(documents[total_documents].path));
                documents[total_documents].valid=1;
                total_documents++;
                printf("[server-log] document%d: title: %s, author: %s, year: %d, path: %s\n", total_documents, task.title, task.authors, task.year, task.path);

                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    char response[128];
                    snprintf(response, sizeof(response), "Document %d indexed\n", total_documents);
                    write(client_fifo, response, strlen(response));
                    close(client_fifo);
                }
            }

            if (task.type == 'c') {
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    char response[512];
                    if (task.id > 0 && task.id <= total_documents && documents[task.id-1].valid) {
                        Document doc = documents[task.id - 1];
                        printf("[server-log] consulting document%d\n", task.id);
                        snprintf(response, sizeof(response),
                            "Title: %s\n"
                            "Authors: %s\n"
                            "Year: %d\n"
                            "Path: %s\n",
                            doc.title, doc.authors, doc.year, doc.path);
                    } else {
                        snprintf(response, sizeof(response), "Error: Document %d not found\n", task.id);
                    }
                    write(client_fifo, response, strlen(response));
                    close(client_fifo);
                }
            } 
            if (task.type == 'd') {
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    char response[128];
                    char* deleted_path = documents[task.id-1].valid ? remove_document(task.id, documents, &total_documents, available_indexs) : NULL;
                    
                    if (deleted_path) {
                        printf("[server-log] deleting document%d (%s)\n", task.id, deleted_path);
                        snprintf(response, sizeof(response), "Index entry %d deleted\n", task.id);
                    } else {
                        snprintf(response, sizeof(response), "Error: Document %d not found\n", task.id);
                    }
                    write(client_fifo, response, strlen(response));
                    close(client_fifo);
                }
            }

            if (task.type == 'l') {
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    char response[128];
                    if (task.id > 0 && task.id <= total_documents && documents[task.id-1].valid) {
                        Document doc = documents[task.id-1];
                        char full_path[256];
                        snprintf(full_path, sizeof(full_path), "%s/%s", argv[1], doc.path);
                        
                        int total_lines = count_line_w_keyword(full_path, task.keyword);
                        printf("[server-log] couting keyword \"%s\" in document%d\n", task.keyword, task.id);
                        snprintf(response, sizeof(response), "%d\n", total_lines);
                    } else {
                        snprintf(response, sizeof(response), "Error: Document %d not found\n", task.id);
                    }
                    
                    write(client_fifo, response, strlen(response));
                    close(client_fifo);
                }
            }

            if (task.type=='s'){
                if (!task.nr_processes){
                    char *output = match_pattern(documents, &total_documents, task.keyword, argv[1]);
                    printf("[server-log] looking for documents with keyword \"%s\"\n", task.keyword);
                    int client_fifo = open(task.client_fifo, O_WRONLY);
                    if (client_fifo != -1) {
                        char response[1000000];
                        snprintf(response, sizeof(response), "[%s", output);
                        strcat(response, "]\n");
                        write(client_fifo, response, strlen(response));
                        close(client_fifo);
                    }
                }
                if (task.nr_processes) {
                    int pipes[task.nr_processes][2];
                    pid_t pids[task.nr_processes];
                    
                    for (int i = 0; i < task.nr_processes; i++) {
                        pipe(pipes[i]);
                        
                        if ((pids[i] = fork()) == 0) {
                            close(pipes[i][0]);
                            int docs_per_process = total_documents / task.nr_processes;
                            int start = i * docs_per_process;
                            int end = (i == task.nr_processes - 1) ? total_documents : start + docs_per_process;
                            int count = end - start;
                            
                            char *partial_output = match_pattern(documents + start, &count, task.keyword, argv[1]);
                            
                            write(pipes[i][1], &start, sizeof(int));  // Envia o offset
                            int output_len = strlen(partial_output);
                            write(pipes[i][1], &output_len, sizeof(int));
                            
                            if (output_len > 0) {
                                write(pipes[i][1], partial_output, output_len + 1);
                            }
                            close(pipes[i][1]);
                            exit(0);
                        }
                        else {
                            close(pipes[i][1]);
                        }
                    }
                    
                    char final_output[1000000] = "[";
                    int first_result = 1;
                    
                    for (int i = 0; i < task.nr_processes; i++) {
                        int start, output_len;
                        read(pipes[i][0], &start, sizeof(int));
                        read(pipes[i][0], &output_len, sizeof(int));
                        
                        if (output_len > 0) {
                            char buffer[1024];
                            read(pipes[i][0], buffer, output_len + 1);
                            
                            char *token = strtok(buffer, ",");
                            while (token != NULL) {
                                if (!first_result) {
                                    strcat(final_output, ",");
                                }
                                
                                int relative_id = atoi(token);
                                int absolute_id = start + relative_id;
                                
                                char id_str[10];
                                snprintf(id_str, sizeof(id_str), "%d", absolute_id);
                                strcat(final_output, id_str);
                                
                                first_result = 0;
                                token = strtok(NULL, ",");
                            }
                        }
                        close(pipes[i][0]);
                        waitpid(pids[i], NULL, 0);
                    }
                    
                    strcat(final_output, "]\n");
                    int client_fifo = open(task.client_fifo, O_WRONLY);
                    if (client_fifo != -1) {
                        write(client_fifo, final_output, strlen(final_output) + 1);
                        close(client_fifo);
                    }
                }
            }

            if (task.type == 'f') {
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    char response[128];
                    snprintf(response, sizeof(response), "Server is shutting down\n");
                    write(client_fifo, response, strlen(response));
                    close(client_fifo);
                }

                printf("[server-log] shutdowning server..\n");

                save_metadata(META_FILE, documents, total_documents);
            
                close(server_fifo);
                unlink(SERVER);
                
                exit(0);
            }
        }
    }
    return 0;
}