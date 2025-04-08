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
#include "utils.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        char* info = "Usage: <document_folder> <cache_size>\n";
        write(STDOUT_FILENO, info, strlen(info));
        return 1;
    }

    int cache_size = atoi(argv[2]);

    Document documents[cache_size];
    int total_documents = 0, loaded=0;

    if (load_metadata(META_FILE, documents, cache_size, &loaded, &total_documents) == 0) {
        cache_size = loaded;
    }
    if (loaded<=cache_size) printf("[server-log] loaded %d from disk..\n", loaded);

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
            time_t now = time(NULL);
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
                if ((try_id = try_insert(task, documents, available_indexs, cache_size, &total_documents))!=0) {
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
                int after_cached;
                if ((after_cached = cache_files(task, documents, &total_documents, cache_size)) !=0){
                    printf("[server-log] document%d: title: %s, author: %s, year: %d, path: %s\n", after_cached, task.title, task.authors, task.year, task.path);
                    int client_fifo = open(task.client_fifo, O_WRONLY);
                    if (client_fifo != -1) {
                        char response[128];
                        snprintf(response, sizeof(response), "Document %d indexed\n", after_cached);
                        write(client_fifo, response, sizeof(response));
                        close(client_fifo);
                    }
                    continue;
                }
                printf("[server-log] couldnt index file\n");
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    write(client_fifo, "Error: Indexing file\n", 21);
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
                documents[total_documents].id = total_documents+1;
                strncpy(documents[total_documents].title, task.title, sizeof(documents[total_documents].title));
                strncpy(documents[total_documents].authors, task.authors, sizeof(documents[total_documents].authors));
                documents[total_documents].year = task.year;
                strncpy(documents[total_documents].path, task.path, sizeof(documents[total_documents].path));
                documents[total_documents].valid=1;
                save_metadata(META_FILE, &documents[total_documents], &total_documents,0);
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
                    int found = 0;
                    int index = -1;

                    for (int i = 0; i < cache_size; i++) {
                        if (documents[i].id == task.id && documents[i].valid) {
                            index = i;
                            found = 1;
                            break;
                        }
                    }
                    
                    if (!found && task.id<=total_documents) {
                        index = load_from_metadata(META_FILE, documents, task.id, cache_size, &total_documents);
                        if (index != -1) {
                            found = 1;
                        }
                    }
                    
                    if (found) {
                        double hours_since_last_access = difftime(now, documents[index].last_access) / 3600.0;
                        documents[index].used *= exp(-hours_since_last_access / 24.0);
                        documents[index].used += 1;
                        documents[index].last_access = now;

                        printf("[server-log] consulting document%d\n", task.id);
                        snprintf(response, sizeof(response),
                            "Title: %s\n"
                            "Authors: %s\n"
                            "Year: %d\n"
                            "Path: %s\n", documents[index].title, documents[index].authors, documents[index].year, documents[index].path);
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
                    int found = 0;
                    int index = -1;
            
                    for (int i = 0; i < cache_size; i++) {
                        if (documents[i].id == task.id && documents[i].valid) {
                            index = i;
                            found = 1;
                            break;
                        }
                    }
                    
                    if (!found && task.id <= total_documents) {
                        index = load_from_metadata(META_FILE, documents, task.id, cache_size, &total_documents);
                        if (index != -1) {
                            found = 1;
                        }
                    }
                    
                    if (found) {
                        char* deleted_path = remove_document(task.id,index, documents, &total_documents, available_indexs);
                        update_metadata(META_FILE, task.id);
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
                    int found = 0;
                    int index = -1;
            
                    for (int i = 0; i < cache_size; i++) {
                        if (documents[i].id == task.id && documents[i].valid) {
                            index = i;
                            found = 1;
                            break;
                        }
                    }
                    
                    if (!found && task.id <= total_documents) {
                        index = load_from_metadata(META_FILE, documents, task.id, cache_size, &total_documents);
                        if (index != -1) {
                            found = 1;
                        }
                    }
                    
                    if (found) {
                        Document doc = documents[index];
                        char full_path[256];
                        snprintf(full_path, sizeof(full_path), "%s/%s", argv[1], doc.path);
                        
                        int total_lines = count_line_w_keyword(full_path, task.keyword);
                        double hours_since_last_access = difftime(now, documents[index].last_access) / 3600.0;
                        documents[index].used *= exp(-hours_since_last_access / 24.0);
                        documents[index].used += 1;
                        documents[index].last_access = now;
                        
                        printf("[server-log] counting keyword \"%s\" in document%d\n", task.keyword, task.id);
                        snprintf(response, sizeof(response), "%d\n", total_lines);
                    } else {
                        snprintf(response, sizeof(response), "Error: Document %d not found\n", task.id);
                    }
                    
                    write(client_fifo, response, strlen(response));
                    close(client_fifo);
                }
            }

            if (task.type == 's') {
                printf("[server-log] looking for documents with keyword \"%s\"\n", task.keyword);
                char* output;
                if (!task.nr_processes)
                    output = match_pattern(documents, &total_documents, task.keyword, argv[1], cache_size, 1);

                if (task.nr_processes) 
                    output = match_pattern(documents, &total_documents, task.keyword, argv[1], cache_size, task.nr_processes);

                output = remove_duplicates(output);

                char response[1000000];
                snprintf(response, sizeof(response), "[%s]\n", output);
                int client_fifo = open(task.client_fifo, O_WRONLY);
                if (client_fifo != -1) {
                    write(client_fifo, response, strlen(response) + 1);
                    close(client_fifo);
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
            
                close(server_fifo);
                unlink(SERVER);
                
                exit(0);
            }
        }
    }
    return 0;
}