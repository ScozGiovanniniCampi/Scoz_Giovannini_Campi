#include "util.h"
#include <stdlib.h>

void add_book_to_vector_normal(const Book *book) {
    if (global_book_vector->size >= global_book_vector->capacity) {
        size_t new_capacity = global_book_vector->capacity == 0 ? 4 : global_book_vector->capacity * 2;
        Book *new_data = realloc(global_book_vector->data, new_capacity * sizeof(Book));
        if (!new_data) {
            return;
        }
        global_book_vector->data = new_data;
        global_book_vector->capacity = new_capacity;
    }
    global_book_vector->data[global_book_vector->size++] = *book;
}

void add_book_to_vector_borrowed(const BorrowedBook *borrowedBook) {
    if (global_borrowed_book_vector->size >= global_borrowed_book_vector->capacity) {
        size_t new_capacity = global_borrowed_book_vector->capacity == 0 ? 4 : global_borrowed_book_vector->capacity * 2;
        BorrowedBook *new_data = realloc(global_borrowed_book_vector->data, new_capacity * sizeof(BorrowedBook));
        if (!new_data) {
            return;
        }
        global_borrowed_book_vector->data = new_data;
        global_borrowed_book_vector->capacity = new_capacity;
    }
    global_borrowed_book_vector->data[global_borrowed_book_vector->size++] = *borrowedBook;
}

Book* remove_book_from_vector_normal(size_t index) {
    if (index >= global_book_vector->size) {
        return NULL; // Index out of bounds
    }
    Book *removed_book = malloc(sizeof(Book));
    if (!removed_book) {
        return NULL; // Memory allocation failed
    }
    *removed_book = global_book_vector->data[index];
    for (size_t i = index; i < global_book_vector->size - 1; ++i) {
        global_book_vector->data[i] = global_book_vector->data[i + 1];
    }
    global_book_vector->size--;
    return removed_book;
}

BorrowedBook* remove_book_from_vector_borrowed(size_t index) {
    if (index >= global_borrowed_book_vector->size) {
        return NULL; // Index out of bounds
    }
    BorrowedBook *removed_book = malloc(sizeof(BorrowedBook));
    if (!removed_book) {
        return NULL; // Memory allocation failed
    }
    *removed_book = global_borrowed_book_vector->data[index];
    for (size_t i = index; i < global_borrowed_book_vector->size - 1; ++i) {
        global_borrowed_book_vector->data[i] = global_borrowed_book_vector->data[i + 1];
    }
    global_borrowed_book_vector->size--;
    return removed_book;
}

void free_book_vector_normal() {
    if (global_book_vector) {
        free(global_book_vector->data);
        global_book_vector->data = NULL;
        global_book_vector->size = 0;
        global_book_vector->capacity = 0;
    }
}

void free_borrowed_book_vector() {
    if (global_borrowed_book_vector) {
        free(global_borrowed_book_vector->data);
        global_borrowed_book_vector->data = NULL;
        global_borrowed_book_vector->size = 0;
        global_borrowed_book_vector->capacity = 0;
    }
}

bool add_user_to_vector(const RegisteredUser *user) {
    if (global_user_vector->size >= global_user_vector->capacity) {
        size_t new_capacity = global_user_vector->capacity == 0 ? 4 : global_user_vector->capacity * 2;
        RegisteredUser *new_data = realloc(global_user_vector->data, new_capacity * sizeof(RegisteredUser));
        if (!new_data) {
            return false;
        }
        global_user_vector->data = new_data;
        global_user_vector->capacity = new_capacity;
    }
    global_user_vector->data[global_user_vector->size++] = *user;

    return true;
}

RegisteredUser* remove_user_from_vector(size_t index) {
    if (index >= global_user_vector->size) {
        return NULL; // Index out of bounds
    }
    RegisteredUser *removed_user = malloc(sizeof(RegisteredUser));
    if (!removed_user) {
        return NULL; // Memory allocation failed
    }
    *removed_user = global_user_vector->data[index];
    for (size_t i = index; i < global_user_vector->size - 1; ++i) {
        global_user_vector->data[i] = global_user_vector->data[i + 1];
    }
    global_user_vector->size--;
    return removed_user;
}

void free_user_vector() {
    if (global_user_vector) {
        free(global_user_vector->data);
        global_user_vector->data = NULL;
        global_user_vector->size = 0;
        global_user_vector->capacity = 0;
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
