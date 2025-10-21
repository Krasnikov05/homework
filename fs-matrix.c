#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

typedef struct {
  int n;
  int *values;
}
matrix_t;

char *create_temp_dir() {
  char template[] = "/tmp/matrixXXXXXX";
  char *tempdir = mkdtemp(template);
  if (!tempdir) {
    perror("mkdtemp");
    exit(1);
  }
  tempdir = strdup(tempdir);
  if (tempdir == NULL) {
    perror("strdup");
    exit(1);
  }
  return tempdir;
}

void remove_temp_dir(const char *path, int n) {
  char filename[256];
  for (int i = 0; i < n; i++) {
    snprintf(filename, sizeof(filename), "%s/row_%d", path, i);
    unlink(filename);
  }
  rmdir(path);
}

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

void compute_row(matrix_t a, matrix_t b, int row, char *tempdir) {
  int result[a.n];
  for (int i = 0; i < a.n; i++) {
    result[i] = 0;
    for (int j = 0; j < a.n; j++) {
      result[i] += a.values[row*a.n + j] * b.values[j*a.n + i];
    }
  }
  char filename[256];
  sprintf(filename, "%s/row_%d", tempdir, row);
  FILE *f = fopen(filename, "wb");
  if (!f) {
    perror("fopen");
    exit(1);
  }
  fwrite(result, sizeof(int), a.n, f);
  fclose(f);
}

matrix_t load_matrix(int n, char *tempdir) {
  matrix_t matrix = create_matrix(n);
  char filename[256];
  for (int i = 0; i < n; i++) {
    sprintf(filename, "%s/row_%d", tempdir, i);
    FILE *f = fopen(filename, "rb");
    if(fread(&matrix.values[i*n], sizeof(int), n, f) != n) {
      perror("fread");
      exit(1);
    }
    fclose(f);
  }
  return matrix;
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
  char *tempdir = create_temp_dir();
  printf("Temporary directory: %s\n", tempdir);

  matrix_t a = create_matrix(n);
  randomize_matrix(a);
  printf("A =\n");
  print_matrix(a);

  matrix_t b = create_matrix(n);
  randomize_matrix(b);
  printf("B =\n");
  print_matrix(b);

  for (int i = 0; i < n; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    }
    if (pid == 0) {
      compute_row(a, b, i, tempdir);
      return 0;
    }
  }

  for (int i = 0; i < n; i++) {
    wait(NULL);
  }

  matrix_t c = load_matrix(n, tempdir);
  printf("A x B =\n");
  print_matrix(c);

  remove_temp_dir(tempdir, n);
  return 0;
}
