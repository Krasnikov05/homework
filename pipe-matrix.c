#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <poll.h>

#define MAX_PROCESS 16

typedef struct {
  int n;
  int *values;
}
matrix_t;

matrix_t create_matrix(int n) {
  matrix_t matrix;
  matrix.n = n;
  matrix.values = calloc(n*n, sizeof(int));
  if (matrix.values == NULL) {
    perror("calloc");
    exit(1);
  }
  return matrix;
}

void randomize_matrix(matrix_t matrix) {
  int size = matrix.n * matrix.n;
  for (int i = 0; i < size; i++) {
    matrix.values[i] = rand() % 20 - 10;
  }
}

void print_matrix(matrix_t matrix) {
  for (int i = 0; i < matrix.n; i++) {
    printf(i == 0 ? "[[" : " [");
    for (int j = 0; j < matrix.n; j++) {
      printf("%4d ", matrix.values[i*matrix.n + j]);
    }
    printf("]");
    if (i != matrix.n - 1) {
      printf("\n");
    }
  }
  printf("]\n");
}

void compute_row(matrix_t a, matrix_t b, int row, int fd) {
  int result[a.n + 1];
  result[0] = row;
  for (int i = 0; i < a.n; i++) {
    result[i + 1] = 0;
    for (int j = 0; j < a.n; j++) {
      result[i + 1] += a.values[row*a.n + j] * b.values[j*a.n + i];
    }
  }
  write(fd, result, (a.n + 1)*sizeof(int));
}

void read_row(matrix_t c, int fd) {
  int buffer[c.n + 1];
  if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
    exit(1);
  }
  memcpy(&c.values[c.n*buffer[0]], &buffer[1], c.n*sizeof(int));
}

int main(int argc, char **argv) {
  // n - и ширина, и высота
  int n = 5;
  if (argc > 1) {
    n = atoi(argv[1]);
  }
  if (n < 0) {
    fprintf(stderr, "Invalid matrix size");
    return 1;
  }
  srand(time(NULL));

  matrix_t a = create_matrix(n);
  randomize_matrix(a);
  printf("A =\n");
  print_matrix(a);

  matrix_t b = create_matrix(n);
  randomize_matrix(b);
  printf("B =\n");
  print_matrix(b);

  int processes = MAX_PROCESS;
  if (n < processes) {
    processes = n;
  }

  int rows = n;
  int pipefd[processes][2];
  for (int i = 0; i < processes; i++) {
    pipe(pipefd[i]);

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    }
    if (pid == 0) {
      for (int j = 0; j < i; j++) {
        close(pipefd[j][0]);
        close(pipefd[j][1]);
      }
      close(pipefd[i][0]);
      for (int row = i; row < n; row += MAX_PROCESS) {
        compute_row(a, b, row, pipefd[i][1]);
      }
      return 0;
    }
    close(pipefd[i][1]);
  }

  matrix_t c = create_matrix(n);
  
  nfds_t nfds = processes;
  struct pollfd pfds[processes];
  for (nfds_t i = 0; i < nfds; i++) {
    pfds[i].fd = pipefd[i][0];
    pfds[i].events = POLLIN;
  }

  while (rows > 0) {
    int ready = poll(pfds, nfds, -1);
    if (ready == -1) {
      perror("poll");
      return 1;
    }
    for (nfds_t i = 0; i < nfds; i++) {
      if(pfds[i].revents & POLLIN) {
        read_row(c, pfds[i].fd);
        rows--;
      }
    }
  }

  printf("A x B =\n");
  print_matrix(c);

  return 0;
}
