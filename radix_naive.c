#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int getMax(int *arr, size_t n) {
  int max = arr[0];
  for (size_t i = 1; i < n; i++) {
    if (arr[i] > max) {
      max = arr[i];
    }
  }
  return max;
}

void countingSort(int *arr, size_t n, int exp) {
  int *output = (int *)malloc(n * sizeof(int));
  int count[10] = {0};

  for (size_t i = 0; i < n; i++) {
    count[(arr[i] / exp) % 10]++;
  }

  for (int i = 1; i < 10; i++) {
    count[i] += count[i - 1];
  }

  for (int i = n - 1; i >= 0; i--) {
    output[count[(arr[i] / exp) % 10] - 1] = arr[i];
    count[(arr[i] / exp) % 10]--;
  }

  for (size_t i = 0; i < n; i++) {
    arr[i] = output[i];
  }

  free(output);
}

void radixSort(int *arr, size_t n) {
  int max = getMax(arr, n);

  for (int exp = 1; max / exp > 0; exp *= 10) {
    countingSort(arr, n, exp);
  }
}

int main() {
  const char *input_filename = "random_numbers.txt";

  // Open the input file
  int fd = open(input_filename, O_RDWR);
  if (fd == -1) {
    perror("Failed to open input file");
    return EXIT_FAILURE;
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    perror("Failed to get file size");
    close(fd);
    return EXIT_FAILURE;
  }
  size_t file_size = sb.st_size;

  // Memory-map the file
  int *data = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    perror("Failed to map file to memory");
    close(fd);
    return EXIT_FAILURE;
  }
  close(fd);

  // Lock the memory to ensure it stays in RAM
  if (mlock(data, file_size) != 0) {
    perror("Failed to lock memory");
    munmap(data, file_size);
    return EXIT_FAILURE;
  }

  printf("File contents pinned in memory successfully. Total integers: %zu\n",
         file_size / sizeof(int));

  // Sort the data
  size_t num_elements = file_size / sizeof(int);
  radixSort(data, num_elements);

  printf("Sorting complete.\n");

  // Unlock and unmap memory
  if (munlock(data, file_size) != 0) {
    perror("Failed to unlock memory");
  }
  if (munmap(data, file_size) != 0) {
    perror("Failed to unmap memory");
  }

  return EXIT_SUCCESS;
}
