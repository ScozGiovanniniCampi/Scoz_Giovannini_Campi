#ifndef UTIL_H
#define UTIL_H

#include "book.h"
#include "messages.h"
#include "user.h"

extern BookVector* global_book_vector = NULL;
extern BorrowedBookVector* global_borrowed_book_vector = NULL;
extern RegisteredUserVector* global_user_vector = NULL;

void add_book_to_vector_normal(const Book *book);
void add_book_to_vector_borrowed(const BorrowedBook *borrowedBook);
Book* remove_book_from_vector_normal(size_t index);
BorrowedBook* remove_book_from_vector_borrowed(size_t index);
void free_book_vector_normal();
void free_borrowed_book_vector();

void add_user_to_vector(const RegisteredUser *user);
RegisteredUser* remove_user_from_vector(size_t index);
void free_user_vector();

size_t read_argument(int fd, char *buffer);
OperationType read_operator(int fd);

#define add_book_to_vector(x, y) _Generic((x), \
    BookVector*: add_book_to_vector_normal, \
    BorrowedBookVector*: add_book_to_vector_borrowed \
)(x, y)

#define free_book_vector(x) _Generic((x), \
    BookVector*: free_book_vector_normal, \
    BorrowedBookVector*: free_borrowed_book_vector \
)(x)

#endif // UTIL_H
