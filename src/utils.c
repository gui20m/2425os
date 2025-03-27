#include "utils.h"

void createFIFO(char* fifo_name) {
    mkfifo(fifo_name, 0666);
}

char* remove_document(int id, Document docs[], int *total) {
  static char path[256];
  if (id < 1 || id > *total) return NULL;
  
  strcpy(path, docs[id-1].path); // Guarda o path antes de remover
  
  for (int i = id-1; i < *total-1; i++) {
      docs[i] = docs[i+1];
  }
  (*total)--;
  
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