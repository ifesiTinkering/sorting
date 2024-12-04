#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

// Cache and block size constants
#define CACHE_LINE_SIZE 64 // 64 bytes
#define ELEMENT_SIZE sizeof(int)
#define BLOCK_SIZE (CACHE_LINE_SIZE / ELEMENT_SIZE * 4) // 4 cache lines per block

// Get the maximum value in the array
int getMax(int *arr, size_t n) {
    int max = arr[0];
    for (size_t i = 1; i < n; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

// Counting sort for a specific digit (with aligned output)
void countingSortBlocked(int *arr, size_t n, int exp, int *output) {
    int count[10] = {0};

    // Count occurrences of each digit
    for (size_t i = 0; i < n; i++) {
        count[(arr[i] / exp) % 10]++;
        __builtin_prefetch(&arr[i + 1], 0, 1); // Prefetch next element
    }

    // Update count[i] to hold the position of the next element with digit i
    for (int i = 1; i < 10; i++) {
        count[i] += count[i - 1];
    }

    // Build the output array
    for (int i = n - 1; i >= 0; i--) {
        int digit = (arr[i] / exp) % 10;
        output[count[digit] - 1] = arr[i];
        count[digit]--;
        __builtin_prefetch(&arr[i - 1], 0, 1); // Prefetch previous element
    }

    // Copy back to original array
    memcpy(arr, output, n * sizeof(int));
}

// Radix sort with cache-line-aware blocking
void radixSortBlocked(int *arr, size_t n) {
    int max = getMax(arr, n);

    // Allocate aligned buffer
    int *buffer = aligned_alloc(CACHE_LINE_SIZE, n * sizeof(int));
    if (!buffer) {
        perror("Failed to allocate aligned buffer");
        exit(EXIT_FAILURE);
    }

    // Perform radix sort
    for (int exp = 1; max / exp > 0; exp *= 10) {
        size_t offset = 0;
        while (offset < n) {
            size_t block_end = offset + BLOCK_SIZE;
            if (block_end > n) {
                block_end = n;
            }

            size_t block_size = block_end - offset;

            // Sort the block for the current digit
            countingSortBlocked(arr + offset, block_size, exp, buffer + offset);

            offset += BLOCK_SIZE;
        }
    }

    free(buffer);
}

// Main function (similar to your original)
int main() {
    const char *input_filename = "random_numbers.txt"; // File containing integers

    // Open the input file
    int fd = open(input_filename, O_RDWR); // Open with read and write permissions
    if (fd == -1) {
        perror("Failed to open input file");
        return EXIT_FAILURE;
    }

    // Get the size of the file
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

    printf("File contents pinned in memory successfully. Total integers: %zu\n", file_size / sizeof(int));

    // Sort the data
    size_t num_elements = file_size / sizeof(int);

    // Measure sorting time
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    radixSortBlocked(data, num_elements);
    clock_gettime(CLOCK_MONOTONIC, &end);

    long long nanoseconds = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    printf("Sorting complete. Time taken: %lld nanoseconds.\n", nanoseconds);

    printf("Sorted data written back to '%s'.\n", input_filename);

    // Unlock and unmap memory
    if (munlock(data, file_size) != 0) {
        perror("Failed to unlock memory");
    }
    if (munmap(data, file_size) != 0) {
        perror("Failed to unmap memory");
    }

    return EXIT_SUCCESS;
}
