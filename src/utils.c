#include "utils.h"

void createFIFO(char* fifo_name) {
    mkfifo(fifo_name, 0666);
}

char* remove_document(int taskid, int id, Document docs[], int *total, int available_indexs[]) {
  static char path[256];
  if (taskid < 1 || taskid > *total) return NULL;
  
  strcpy(path, docs[id].path);
  
  memset(&docs[id], 0, sizeof(Document));
  docs[id].valid = 0;
  available_indexs[id] = 1;
  
  return path;
}

int is_valid_document(char* doc_folder, char* rel_path) {
  char full_path[64];
  int fd;

  if (snprintf(full_path, 64, "%s/%s", doc_folder, rel_path) >= 64) {
      return 0;
  }

  fd = open(full_path, O_RDONLY);
  if (fd == -1) {
      return 0;
  }
  close(fd);

  return 1;
}

static int contains_keyword(char *line, char *keyword) {
  char *ptr = line;
  int kw_len = strlen(keyword);
  int line_len = strlen(line);
  
  for (int i = 0; i <= line_len - kw_len; i++) {
      if (memcmp(ptr + i, keyword, kw_len) == 0) {
          return 1;
      }
  }
  return 0;
}

int count_line_w_keyword(char* path, char* keyword) {
  FILE *file = fopen(path, "r");
  if (!file) return -1;
  
  int count = 0;
  char *line = NULL;
  size_t len = 0;
  
  while (getline(&line, &len, file) != -1) {
      if (contains_keyword(line, keyword)) {
          count++;
      }
  }
  
  free(line);
  fclose(file);
  return count;
}

static int count_keyword_in_file(char* path, char* keyword) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;
    
    char buffer[2048];
    ssize_t bytes_read;
    int count = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        char *start = buffer;
        char *end = buffer + bytes_read;

        while (start < end) {
            char *nl = memchr(start, '\n', end - start);
            size_t line_len = nl ? (nl - start) : (end - start);
            if (memmem(start, line_len, keyword, strlen(keyword))) {
                count++;
            }
            start = nl ? nl + 1 : end;
        }
    }
    close(fd);
    return count;
}

char* match_pattern(Document documents[], int *total_documents, char* keyword, char* route, int cache_size, int num_processes) {
    static char response[1000000];
    response[0] = '\0';
    
    if (num_processes <= 1) {
        int first_match = 1;
        for (int i = 0; i < *total_documents; i++) {
            Document doc;
            int index = i;
            if (i < cache_size) {
                doc = documents[i];
            } else {
                index = load_from_metadata(META_FILE, documents, i+1, cache_size, total_documents);
                if (index == -1) continue;
                doc = documents[index];
            }

            if (!doc.valid) continue;
            documents[index].used++;
            
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", route, doc.path);
            
            int count = count_keyword_in_file(full_path, keyword);
            
            if (count > 0) {
                if (!first_match) strcat(response, ",");
                char entry[32];
                snprintf(entry, sizeof(entry), "%d", doc.id);
                strcat(response, entry);
                first_match = 0;
            }
        }
        return response;
    }

    int pipes[num_processes][2];
    pid_t pids[num_processes];
    int docs_per_process = (*total_documents + num_processes - 1) / num_processes;

    for (int i = 0; i < num_processes; i++) {
        pipe(pipes[i]);
        pids[i] = fork();
        
        if (pids[i] == 0) {
            close(pipes[i][0]);
            
            char partial_response[1000000] = "";
            int first = 1;
            int start = i * docs_per_process;
            int end = (start + docs_per_process > *total_documents) ? *total_documents : start + docs_per_process;
            
            for (int j = start; j < end; j++) {
                Document doc;
                int index = j;
                if (j < cache_size) {
                    doc = documents[j];
                } else {
                    index = load_from_metadata(META_FILE, documents, j+1, cache_size, total_documents);
                    if (index == -1) continue;
                    doc = documents[index];
                }

                if (!doc.valid) continue;
                documents[index].used++;
                
                char full_path[256];
                snprintf(full_path, sizeof(full_path), "%s/%s", route, doc.path);
                
                int count = count_keyword_in_file(full_path, keyword);
                
                if (count > 0) {
                    if (!first) strcat(partial_response, ",");
                    char entry[32];
                    snprintf(entry, sizeof(entry), "%d", doc.id);
                    strcat(partial_response, entry);
                    first = 0;
                }
            }
            
            write(pipes[i][1], partial_response, strlen(partial_response) + 1);
            close(pipes[i][1]);
            exit(0);
        } else {
            close(pipes[i][1]);
        }
    }

    int first = 1;
    for (int i = 0; i < num_processes; i++) {
        char buffer[1000000];
        ssize_t bytes_read = read(pipes[i][0], buffer, sizeof(buffer));
        close(pipes[i][0]);
        waitpid(pids[i], NULL, 0);
        
        if (bytes_read > 0) {
            if (!first && buffer[0] != '\0') {
                strcat(response, ",");
            }
            strcat(response, buffer);
            first = 0;
        }
    }

    return response;
}

void save_metadata(char* filename, Document *documents, int *total_documents, int *valid_index) {
    int fd = open(filename, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    off_t write_pos = sizeof(int) + ((valid_index != NULL) ? (*valid_index * sizeof(Document)) : (*total_documents * sizeof(Document)));
    lseek(fd, write_pos, SEEK_SET);

    if (write(fd, documents, sizeof(Document)) == -1) {
        perror("Error writing document");
        close(fd);
        return;
    }

    if (valid_index == NULL) {
        int updated_total = *total_documents + 1;
        lseek(fd, 0, SEEK_SET);
        write(fd, &updated_total, sizeof(int));
    }

    close(fd);
}


int load_metadata(char* filename, Document docs[], int max_size, int *loaded, int* total_docs) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return -1;

    int file_total;
    if (read(fd, &file_total, sizeof(int)) != sizeof(int)) {
        close(fd);
        return -1;
    }
    
    *total_docs += file_total;
    *loaded = (file_total >= max_size) ? max_size : file_total;
    if (file_total > *loaded) printf("[server-log] cache was truncated for the first entries [%d/%d]\n", *loaded, file_total);

    for (int i = 0; i < *loaded; i++) {
        if (read(fd, &docs[i], sizeof(Document)) != sizeof(Document)) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

void update_metadata(char* filename, int document_id) {
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Error opening metadata file for update");
        return;
    }

    off_t update_pos = sizeof(int) + ((document_id - 1) * sizeof(Document));
    
    if (lseek(fd, update_pos, SEEK_SET) == -1) {
        perror("Error seeking in metadata file");
        close(fd);
        return;
    }

    Document doc;
    if (read(fd, &doc, sizeof(Document)) != sizeof(Document)) {
        perror("Error reading document from metadata");
        close(fd);
        return;
    }

    doc.valid = 0;

    if (lseek(fd, update_pos, SEEK_SET) == -1) {
        perror("Error seeking for write");
        close(fd);
        return;
    }

    if (write(fd, &doc, sizeof(Document)) != sizeof(Document)) {
        perror("Error writing updated document to metadata");
    }

    close(fd);
}

int try_insert(Task task, Document documents[], int available_indexs[], int cache_size, int* total_documents) {
    int num_processes = cache_size/100 > 0 ? cache_size/100 : 1;
    int pipe_fds[num_processes][2];
    pid_t pids[num_processes];
    int chunk_size = cache_size / num_processes;

    for (int i = 0; i < num_processes; i++) {
        pipe(pipe_fds[i]);

        if ((pids[i]=fork())== 0) {
            close(pipe_fds[i][0]);
            
            int start = i * chunk_size;
            int end = (i == num_processes-1) ? cache_size : start + chunk_size;
            int found_index = -1;

            for (int j = start; j < end; j++) {
                if (available_indexs[j] == 1 || documents[j].valid==0) {
                    found_index = j;
                    break;
                }
            }

            write(pipe_fds[i][1], &found_index, sizeof(int));
            close(pipe_fds[i][1]);
            exit(0);
        }
        else {
            close(pipe_fds[i][1]);
        }
    }

    int available_index = -1;
    for (int i = 0; i < num_processes; i++) {
        int child_result;
        read(pipe_fds[i][0], &child_result, sizeof(int));
        
        if (child_result != -1 && available_index == -1) {
            available_index = child_result;
        }
        
        close(pipe_fds[i][0]);
        waitpid(pids[i], NULL, 0);
    }

    if (available_index != -1) {
        documents[available_index].id = ++(*total_documents);
        strncpy(documents[available_index].title, task.title, sizeof(documents[available_index].title));
        strncpy(documents[available_index].authors, task.authors, sizeof(documents[available_index].authors));
        documents[available_index].year = task.year;
        strncpy(documents[available_index].path, task.path, sizeof(documents[available_index].path));
        documents[available_index].valid = 1;
        available_indexs[available_index] = 0;
        save_metadata(META_FILE, &documents[available_index], total_documents, &available_index);
        *total_documents+=1;
        return available_index + 1;
    }

    return 0;
}

int least_used_frequently(Document documents[], int cache_size) {
    int num_processes = cache_size/100 > 0 ? cache_size/100 : 1;
    int segment_size = cache_size / num_processes;
    int pipes[num_processes][2];
    pid_t pids[num_processes];

    for (int i = 0; i < num_processes; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe failed");
            return -1;
        }
    }

    for (int i = 0; i < num_processes; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            close(pipes[i][0]);
            
            int start = i * segment_size;
            int end = (i == num_processes - 1) ? cache_size : start + segment_size;
            
            int min_used = documents[start].used;
            int min_index = start;
            
            for (int j = start + 1; j < end; j++) {
                if (documents[j].used < min_used) {
                    min_used = documents[j].used;
                    min_index = j;
                }
            }
            
            write(pipes[i][1], &min_index, sizeof(int));
            write(pipes[i][1], &min_used, sizeof(int));
            
            close(pipes[i][1]);
            exit(0);
        } else {
            close(pipes[i][1]);
        }
    }

    int least_used_index = 0;
    int least_used_value = documents[0].used;
    
    for (int i = 0; i < num_processes; i++) {
        int current_index, current_used;
        
        read(pipes[i][0], &current_index, sizeof(int));
        read(pipes[i][0], &current_used, sizeof(int));
        
        if (current_used < least_used_value) {
            least_used_index = current_index;
            least_used_value = current_used;
        }
        
        close(pipes[i][0]);
        waitpid(pids[i], NULL, 0);
    }

    return least_used_index;
}

int cache_files(Task task, Document* documents, int* total_documents, int cache_size) {
    int index = least_used_frequently(documents, cache_size);
    if (index == -1) {
        return -1;
    }

    if (documents[index].valid) {
        save_metadata(META_FILE, &documents[index], total_documents, &index);
    }

    documents[index].id = *total_documents + 1;
    strncpy(documents[index].title, task.title, sizeof(documents[index].title));
    strncpy(documents[index].authors, task.authors, sizeof(documents[index].authors));
    documents[index].year = task.year;
    strncpy(documents[index].path, task.path, sizeof(documents[index].path));
    documents[index].valid = 1;
    documents[index].used = 0;
    save_metadata(META_FILE, &documents[index], total_documents, 0);
    (*total_documents)++;
    return *total_documents;
}

int load_from_metadata(char* filename, Document documents[], int id, int cache_size, int *total_docs) {
    printf("usei..\n");
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return -1;
    }

    off_t position = sizeof(int) + (id * sizeof(Document));
    
    if (lseek(fd, position-sizeof(Document), SEEK_SET) == -1) {
        perror("Error seeking file position");
        close(fd);
        return -1;
    }

    int index = least_used_frequently(documents, cache_size);

    if (read(fd, &documents[index], sizeof(Document)) != sizeof(Document)) {
        close(fd);
        return -1;
    }

    if (!documents[index].valid) return -1;

    close(fd);
    return index;
}

static int compare_ints(const void* a, const void* b) {
    int arg1 = *(const int*)a;
    int arg2 = *(const int*)b;
    return (arg1 > arg2) - (arg1 < arg2);
}

char* remove_duplicates(char* input) {
    if (input == NULL || strlen(input) == 0) {
        char* result = malloc(1);
        result[0] = '\0';
        return result;
    }

    int count = 1;
    for (const char* p = input; *p; p++) {
        if (*p == ',') count++;
    }

    int* numbers = malloc(count * sizeof(int));
    int num_count = 0;
    
    const char* start = input;
    const char* end;
    char buffer[32];
    
    while (*start) {
        end = strchr(start, ',');
        if (end == NULL) end = start + strlen(start);
        
        int len = end - start;
        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        
        strncpy(buffer, start, len);
        buffer[len] = '\0';
        
        numbers[num_count++] = atoi(buffer);
        
        if (*end == '\0') break;
        start = end + 1;
    }

    qsort(numbers, num_count, sizeof(int), compare_ints);

    int unique_count = 0;
    for (int i = 0; i < num_count; i++) {
        if (i == 0 || numbers[i] != numbers[i - 1]) {
            numbers[unique_count++] = numbers[i];
        }
    }

    char* result = malloc(strlen(input) + 1);
    result[0] = '\0';
    
    for (int i = 0; i < unique_count; i++) {
        char num_str[32];
        snprintf(num_str, sizeof(num_str), "%d", numbers[i]);
        
        if (i > 0) strcat(result, ",");
        strcat(result, num_str);
    }
    
    free(numbers);
    return result;
}
