#include <fcntl.h>
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define L1_CACHE_SIZE 32 * 1024
#define ELEMENT_SIZE sizeof(int)
#define BLOCK_SIZE (L1_CACHE_SIZE / ELEMENT_SIZE)

int getMax(int *arr, size_t n) {
  __m256i maxVec = _mm256_set1_epi32(arr[0]);

  for (size_t i = 0; i < n; i += 8) {
    if (i + 16 < n) {
      __builtin_prefetch(&arr[i + 16], 0, 1);
    }
    __m256i vec = _mm256_loadu_si256((__m256i *)&arr[i]);
    maxVec = _mm256_max_epi32(maxVec, vec);
  }

  int maxArray[8];
  _mm256_storeu_si256((__m256i *)maxArray, maxVec);
  int max = maxArray[0];
  for (int i = 1; i < 8; i++) {
    if (maxArray[i] > max) {
      max = maxArray[i];
    }
  }

  return max;
}

void countingSortBlocked(int *arr, size_t n, int exp) {
  int *output = (int *)malloc(n * sizeof(int));
  int count[10] = {0};

  for (size_t i = 0; i < n; i++) {
    if (i + 16 < n) {
      __builtin_prefetch(&arr[i + 16], 0, 1);
    }
    count[(arr[i] / exp) % 10]++;
  }

  for (int i = 1; i < 10; i++) {
    count[i] += count[i - 1];
  }

  for (int i = n - 1; i >= 0; i--) {
    output[--count[(arr[i] / exp) % 10]] = arr[i];
  }

  memcpy(arr, output, n * sizeof(int));
  free(output);
}

void radixSortBlocked(int *arr, size_t n) {
  int max = getMax(arr, n);
  int *buffer = (int *)malloc(BLOCK_SIZE * sizeof(int));

  for (int exp = 1; max / exp > 0; exp *= 10) {
    size_t offset = 0;
    while (offset < n) {
      size_t block_end = offset + BLOCK_SIZE;
      if (block_end > n) {
        block_end = n;
      }

      size_t block_size = block_end - offset;
      memcpy(buffer, arr + offset, block_size * sizeof(int));

      countingSortBlocked(buffer, block_size, exp);

      memcpy(arr + offset, buffer, block_size * sizeof(int));

      offset += BLOCK_SIZE;
    }
  }

  free(buffer);
}

int main() {
  const char *input_filename = "random_numbers.txt";

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

  if (file_size % 32 != 0) {
    file_size =
        file_size - (file_size % 32); // Truncate to nearest 32-byte multiple
  }

  int *data = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    perror("Failed to map file to memory");
    close(fd);
    return EXIT_FAILURE;
  }
  close(fd);

  if (mlock(data, file_size) != 0) {
    perror("Failed to lock memory");
    munmap(data, file_size);
    return EXIT_FAILURE;
  }

  printf("File contents pinned in memory successfully. Total integers: %zu\n",
         file_size / sizeof(int));

  size_t num_elements = file_size / sizeof(int);

  radixSortBlocked(data, num_elements);
  printf("Sorting complete.\n");

  if (munlock(data, file_size) != 0) {
    perror("Failed to unlock memory");
  }
  if (munmap(data, file_size) != 0) {
    perror("Failed to unmap memory");
  }

  return EXIT_SUCCESS;
}
