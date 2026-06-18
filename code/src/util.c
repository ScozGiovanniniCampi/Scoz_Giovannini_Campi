#include "util.h"
#include <stdlib.h>

void add_book_to_vector_normal(BookVector *vector, const Book *book) {
    if (vector->size >= vector->capacity) {
        size_t new_capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
        Book *new_data = realloc(vector->data, new_capacity * sizeof(Book));
        if (!new_data) {
            return;
        }
        vector->data = new_data;
        vector->capacity = new_capacity;
    }
    vector->data[vector->size++] = *book;
}

void add_book_to_vector_borrowed(BorrowedBookVector *vector, const BorrowedBook *borrowedBook) {
    if (vector->size >= vector->capacity) {
        size_t new_capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
        BorrowedBook *new_data = realloc(vector->data, new_capacity * sizeof(BorrowedBook));
        if (!new_data) {
            return;
        }
        vector->data = new_data;
        vector->capacity = new_capacity;
    }
    vector->data[vector->size++] = *borrowedBook;
}

void free_book_vector_normal(BookVector *vector) {
    if (vector) {
        free(vector->data);
        vector->data = NULL;
        vector->size = 0;
        vector->capacity = 0;
    }
}

void free_borrowed_book_vector(BorrowedBookVector *vector) {
    if (vector) {
        free(vector->data);
        vector->data = NULL;
        vector->size = 0;
        vector->capacity = 0;
    }
}

size_t read_argument(int fd, char *buffer) {
    buffer = malloc(1); // Allocate memory for the buffer
    char tmp_buffer[1]; // Temporary buffer for reading
    ssize_t bytesRead = 0;
    size_t totalBytesRead = 0;
    while (1) {
        bytesRead = read(fd, tmp_buffer, 1); // Read one byte at a time
        if (bytesRead < 0) {
            break;
        }
        totalBytesRead += bytesRead;
        if (totalBytesRead >= sizeof(buffer)) {
            buffer = realloc(buffer, sizeof(buffer) * 2); // Resize buffer if needed
        }

        if (tmp_buffer[0] == END_OF_ARGUMENT) {
            break; // End of argument
        }
        if (tmp_buffer[0] == END_OF_TRANSMISSION) {
            lseek(fd, -1, SEEK_CUR); // Move back the file descriptor to re-read the EOT character later
            break; // End of transmission
        }

        memcpy(buffer + totalBytesRead, tmp_buffer, bytesRead);
        totalBytesRead += bytesRead;
    }
    buffer = realloc(buffer, totalBytesRead + 1); // Resize buffer to fit the data
    buffer[totalBytesRead] = '\0'; // Null-terminate the string
    return totalBytesRead;
}

OperationType read_operator(int fd) {
    char* buffer = NULL;
    if (read_argument(fd, buffer) == 0) { // No data read, return an error code
        return OP_ERROR; // Indicate an error
    }
    OperationType op_code = atoi(buffer);
    free(buffer);
    return op_code;
}
