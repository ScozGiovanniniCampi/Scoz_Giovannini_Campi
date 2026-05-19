#include "util.h"

#include <string.h>

void add_book_to_hash_normal(Book **books, Book *book) {
    unsigned int book_hash = hash_book(book);
    size_t title_len = strnlen(book->title, MAX_TITLE_LENGTH);
    HASH_ADD_KEYPTR_BYHASHVALUE(hh, *books, book->title, title_len, book_hash, book);
}

void add_book_to_hash_borrowed(BorrowedBook **borrowedBooks, BorrowedBook *borrowedBook) {
    unsigned int book_hash = hash_book(&borrowedBook->book);
    size_t title_len = strnlen(borrowedBook->book.title, MAX_TITLE_LENGTH);
    HASH_ADD_KEYPTR_BYHASHVALUE(hh, *borrowedBooks, borrowedBook->book.title, title_len, book_hash, borrowedBook);
}
