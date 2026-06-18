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

void free_book_vector(BookVector *vector) {
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
