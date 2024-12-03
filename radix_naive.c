#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h> // For measuring time

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

// Overwrite the same file with sorted data
int writeToSameFile(const char *filename, int *data, size_t num_elements) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file for writing");
        return -1;
    }

    for (size_t i = 0; i < num_elements; i++) {
        if (fprintf(file, "%d\n", data[i]) < 0) {
            perror("Failed to write to file");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

int main() {
    const char *input_filename = "random_numbers.txt"; // File containing 3 GB of integers

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
    radixSort(data, num_elements);
    clock_gettime(CLOCK_MONOTONIC, &end);

    long long nanoseconds = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    printf("Sorting complete. Time taken: %lld nanoseconds.\n", nanoseconds);

    // // Overwrite the same file with sorted data
    // printf("Writing sorted data back to the same file...\n");
    // if (writeToSameFile(input_filename, data, num_elements) != 0) {
    //     fprintf(stderr, "Failed to write sorted data to file\n");
    //     munlock(data, file_size);
    //     munmap(data, file_size);
    //     return EXIT_FAILURE;
    // }

    // printf("Sorted data written back to '%s'.\n", input_filename);

    // Unlock and unmap memory
    if (munlock(data, file_size) != 0) {
        perror("Failed to unlock memory");
    }
    if (munmap(data, file_size) != 0) {
        perror("Failed to unmap memory");
    }

    return EXIT_SUCCESS;
}
