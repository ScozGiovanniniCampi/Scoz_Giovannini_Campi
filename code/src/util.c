#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Definitions of the global vectors
BookVector global_book_vector = {0};
BorrowedBookVector global_borrowed_book_vector = {0};
RegisteredUserVector global_user_vector = {0};

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

Book *remove_book_from_vector_normal(BookVector *vector, size_t index) {
    if (index >= vector->size) {
        return NULL;  // Index out of bounds
    }
    Book *removed_book = malloc(sizeof(Book));
    if (!removed_book) {
        return NULL;  // Memory allocation failed
    }
    *removed_book = vector->data[index];
    for (size_t i = index; i < vector->size - 1; ++i) {
        vector->data[i] = vector->data[i + 1];
    }
    vector->size--;
    return removed_book;
}

BorrowedBook *remove_book_from_vector_borrowed(BorrowedBookVector *vector, size_t index) {
    if (index >= vector->size) {
        return NULL;  // Index out of bounds
    }
    BorrowedBook *removed_book = malloc(sizeof(BorrowedBook));
    if (!removed_book) {
        return NULL;  // Memory allocation failed
    }
    *removed_book = vector->data[index];
    for (size_t i = index; i < vector->size - 1; ++i) {
        vector->data[i] = vector->data[i + 1];
    }
    vector->size--;
    return removed_book;
}

void free_book_vector_normal(BookVector *vector) {
    free(vector->data);
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
}

void free_borrowed_book_vector(BorrowedBookVector *vector) {
    free(vector->data);
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
}

bool add_user_to_vector(const RegisteredUser *user) {
    if (global_user_vector.size >= global_user_vector.capacity) {
        size_t new_capacity =
            global_user_vector.capacity == 0 ? 4 : global_user_vector.capacity * 2;
        RegisteredUser *new_data =
            realloc(global_user_vector.data, new_capacity * sizeof(RegisteredUser));
        if (!new_data) {
            return false;
        }
        global_user_vector.data = new_data;
        global_user_vector.capacity = new_capacity;
    }
    global_user_vector.data[global_user_vector.size++] = *user;

    return true;
}

RegisteredUser *remove_user_from_vector(size_t index) {
    if (index >= global_user_vector.size) {
        return NULL;  // Index out of bounds
    }
    RegisteredUser *removed_user = malloc(sizeof(RegisteredUser));
    if (!removed_user) {
        return NULL;  // Memory allocation failed
    }
    *removed_user = global_user_vector.data[index];
    for (size_t i = index; i < global_user_vector.size - 1; ++i) {
        global_user_vector.data[i] = global_user_vector.data[i + 1];
    }
    global_user_vector.size--;
    return removed_user;
}

void free_user_vector() {
    free(global_user_vector.data);
    global_user_vector.data = NULL;
    global_user_vector.size = 0;
    global_user_vector.capacity = 0;
}

size_t read_argument(int fd, char **buffer_out) {
    size_t capacity = 16;
    char *buffer = malloc(capacity);
    if (!buffer) {
        perror("malloc");
        *buffer_out = NULL;
        return 0;
    }

    size_t size = 0;
    char c;
    ssize_t bytesRead;

    while (1) {
        bytesRead = read(fd, &c, 1);
        if (bytesRead <= 0) {
            break;
        }

        if (c == END_OF_ARGUMENT) {
            break;
        }
        if (c == END_OF_TRANSMISSION) {
            lseek(fd, -1, SEEK_CUR);  // Move back to re-read EOT later
            break;
        }

        if (size + 1 >= capacity) {
            capacity *= 2;
            char *new_buf = realloc(buffer, capacity);
            if (!new_buf) {
                perror("realloc");
                free(buffer);
                *buffer_out = NULL;
                return 0;
            }
            buffer = new_buf;
        }
        buffer[size++] = c;
    }

    buffer[size] = '\0';
    *buffer_out = buffer;
    return size;
}

OperationType read_operator(int fd) {
    char *buffer = NULL;
    if (read_argument(fd, &buffer) == 0) {  // No data read, return an error code
        if (buffer) free(buffer);
        return OP_ERROR;  // Indicate an error
    }
    OperationType op_code = atoi(buffer);
    free(buffer);
    return op_code;
}
