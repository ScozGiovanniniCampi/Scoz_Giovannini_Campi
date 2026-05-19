#ifndef UTIL_H
#define UTIL_H

#include "book.h"

void add_book_to_hash_normal(Book **books, Book *book);
void add_book_to_hash_borrowed(BorrowedBook **borrowedBooks, BorrowedBook *borrowedBook);

#define add_book_to_hash(x, y) _Generic((x), \
    Book**: add_book_to_hash_normal, \
    BorrowedBook**: add_book_to_hash_borrowed \
)(x, y)

#endif // UTIL_H
