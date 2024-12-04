#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

// Cache size constants
#define L1_CACHE_SIZE 32 * 1024 // 32 KB
#define ELEMENT_SIZE sizeof(int)
#define BLOCK_SIZE (L1_CACHE_SIZE / ELEMENT_SIZE) // Number of integers per block

// Insertion sort for small chunks
void insertionSort(int *arr, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= left && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// Merge function using shared buffer
void merge(int *arr, int left, int mid, int right, int *tempBuffer) {
    int i = left, j = mid + 1, k = 0;

    // Merge the two halves into the temp buffer
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) {
            tempBuffer[k++] = arr[i++];
        } else {
            tempBuffer[k++] = arr[j++];
        }
    }
    while (i <= mid) tempBuffer[k++] = arr[i++];
    while (j <= right) tempBuffer[k++] = arr[j++];

    // Copy the sorted data back into the original array
    memcpy(arr + left, tempBuffer, (right - left + 1) * sizeof(int));
}

// Merge sort function with blocking
void mergeSortBlocked(int *arr, int left, int right, int *tempBuffer) {
    // Use insertion sort for chunks smaller than BLOCK_SIZE
    if (right - left + 1 <= BLOCK_SIZE) {
        insertionSort(arr, left, right);
        return;
    }

    int mid = left + (right - left) / 2;

    // Recursive calls for left and right halves
    mergeSortBlocked(arr, left, mid, tempBuffer);
    mergeSortBlocked(arr, mid + 1, right, tempBuffer);

    // Merge the two halves
    merge(arr, left, mid, right, tempBuffer);
}

// Iterative merge sort for blocking
void iterativeMergeSort(int *arr, size_t n, int *tempBuffer) {
    // Process blocks iteratively to build sorted subarrays
    for (size_t blockSize = BLOCK_SIZE; blockSize < n; blockSize *= 2) {
        for (size_t left = 0; left < n - blockSize; left += 2 * blockSize) {
            size_t mid = left + blockSize - 1;
            size_t right = (left + 2 * blockSize - 1 < n) ? left + 2 * blockSize - 1 : n - 1;
            merge(arr, left, mid, right, tempBuffer);
        }
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
    const char *input_filename = "random_numbers.txt";

    // Open the input file
    int fd = open(input_filename, O_RDWR);
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

    // Allocate shared buffer for merging
    int *tempBuffer = (int *)malloc(num_elements * sizeof(int));
    if (!tempBuffer) {
        perror("Failed to allocate temporary buffer");
        munlock(data, file_size);
        munmap(data, file_size);
        return EXIT_FAILURE;
    }

    // Measure sorting time
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    iterativeMergeSort(data, num_elements, tempBuffer);
    clock_gettime(CLOCK_MONOTONIC, &end);

    long long nanoseconds = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    printf("Sorting complete. Time taken: %lld nanoseconds.\n", nanoseconds);

    // Overwrite the same file with sorted data
    printf("Writing sorted data back to the same file...\n");
    if (writeToSameFile(input_filename, data, num_elements) != 0) {
        fprintf(stderr, "Failed to write sorted data to file\n");
        free(tempBuffer);
        munlock(data, file_size);
        munmap(data, file_size);
        return EXIT_FAILURE;
    }

    printf("Sorted data written back to '%s'.\n", input_filename);

    // Free temporary buffer
    free(tempBuffer);

    // Unlock and unmap memory
    if (munlock(data, file_size) != 0) {
        perror("Failed to unlock memory");
    }
    if (munmap(data, file_size) != 0) {
        perror("Failed to unmap memory");
    }

    return EXIT_SUCCESS;
}
