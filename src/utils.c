#include "utils.h"

void createFIFO(char* fifo_name) {
    mkfifo(fifo_name, 0666);
}

char* remove_document(int id, Document docs[], int *total) {
  static char path[256];
  if (id < 1 || id > *total) return NULL;
  
  strcpy(path, docs[id-1].path);
  
  memset(&docs[id-1], 0, sizeof(Document));
  docs[id-1].valid = 0;
  
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

char* match_pattern(Document documents[], int *total_documents, char* keyword, char* route) {
  static char response[4096];
  response[0] = '\0';
  int first_match = 1;

  for (int i = 0; i < *total_documents; i++) {
      Document doc = documents[i];
      if (!doc.valid) continue;
      char full_path[1024];
      snprintf(full_path, sizeof(full_path), "%s/%s", route, doc.path);
      
      int fd = open(full_path, O_RDONLY);
      if (fd == -1) continue;

      char buffer[4096];
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

              if (nl) {
                  start = nl + 1;
              } else {
                  break;
              }
          }
      }

      close(fd);

      if (count > 0) {
          if (!first_match) strcat(response, ",");
          char entry[256];
          snprintf(entry, sizeof(entry), "%d", i + 1);
          strcat(response, entry);
          first_match = 0;
      }
  }
  return response;
}

void save_metadata(const char* filename, Document docs[], int total) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("Error opening metadata file");
        return;
    }

    if (write(fd, &total, sizeof(int)) == -1) {
        perror("Error writing total");
    }

    for (int i = 0; i < total; i++) {
        if (write(fd, &docs[i], sizeof(Document)) == -1) {
            perror("Error writing document");
        }
    }

    close(fd);
}

int load_metadata(const char* filename, Document docs[], int max_size, int *loaded) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return -1;

    int file_total;
    if (read(fd, &file_total, sizeof(int)) != sizeof(int)) {
        close(fd);
        return -1;
    }

    *loaded = (file_total > max_size) ? max_size : file_total;
    if (file_total > *loaded) printf("[server-log] cache was truncated for the first %d entries..\n", *loaded);
    if (max_size>*loaded) printf("[server-log] loaded %d from disk..\n", file_total);

    for (int i = 0; i < *loaded; i++) {
        if (read(fd, &docs[i], sizeof(Document)) != sizeof(Document)) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}