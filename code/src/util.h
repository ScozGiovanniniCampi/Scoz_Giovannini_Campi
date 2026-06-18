#ifndef UTIL_H
#define UTIL_H

#include "book.h"

void add_book_to_vector_normal(BookVector *vector, const Book *book);
void add_book_to_vector_borrowed(BorrowedBookVector *vector, const BorrowedBook *borrowedBook);
void free_book_vector(BookVector *vector);
void free_borrowed_book_vector(BorrowedBookVector *vector);

#define add_book_to_vector(x, y) _Generic((x), \
    BookVector*: add_book_to_vector_normal, \
    BorrowedBookVector*: add_book_to_vector_borrowed \
)(x, y)

#define free_book_vector(x) _Generic((x), \
    BookVector*: free_book_vector, \
    BorrowedBookVector*: free_borrowed_book_vector \
)(x)

#endif // UTIL_H
