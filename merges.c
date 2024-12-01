#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h> // For measuring time

// Merge function for merge sort
void merge(int *arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int *leftArr = (int *)malloc(n1 * sizeof(int));
    int *rightArr = (int *)malloc(n2 * sizeof(int));

    for (int i = 0; i < n1; i++) leftArr[i] = arr[left + i];
    for (int j = 0; j < n2; j++) rightArr[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (leftArr[i] <= rightArr[j]) {
            arr[k++] = leftArr[i++];
        } else {
            arr[k++] = rightArr[j++];
        }
    }
    while (i < n1) arr[k++] = leftArr[i++];
    while (j < n2) arr[k++] = rightArr[j++];

    free(leftArr);
    free(rightArr);
}

// Merge sort function
void mergeSort(int *arr, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSort(arr, left, mid);
        mergeSort(arr, mid + 1, right);
        merge(arr, left, mid, right);
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
    mergeSort(data, 0, num_elements - 1);
    clock_gettime(CLOCK_MONOTONIC, &end);

    long long nanoseconds = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    printf("Sorting complete. Time taken: %lld nanoseconds.\n", nanoseconds);

    // Overwrite the same file with sorted data
    printf("Writing sorted data back to the same file...\n");
    if (writeToSameFile(input_filename, data, num_elements) != 0) {
        fprintf(stderr, "Failed to write sorted data to file\n");
        munlock(data, file_size);
        munmap(data, file_size);
        return EXIT_FAILURE;
    }

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
