#include "utils.h"

void createFIFO(char* fifo_name) {
    mkfifo(fifo_name, 0666);
}

int remove_document(int id, Document documents[], int *total_documents) {
  if (id < 1 || id > *total_documents) return 0;
  
  for (int i = id - 1; i < *total_documents - 1; i++) {
      documents[i] = documents[i + 1];
  }
  (*total_documents)--;
  return 1;
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