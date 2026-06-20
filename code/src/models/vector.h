#ifndef VECTOR_H
#define VECTOR_H

#include "book.h"
#include "user.h"

extern BookVector global_book_vector;
extern BorrowedBookVector global_borrowed_book_vector;
extern RegisteredUserVector global_user_vector;

// BookVector operations
void add_book_to_vector_normal(BookVector* vector, const Book* book);
Book* remove_book_from_vector_normal(BookVector* vector, size_t index);
void free_book_vector_normal(BookVector* vector);

// BorrowedBookVector operations
void add_book_to_vector_borrowed(BorrowedBookVector* vector, const BorrowedBook* borrowedBook);
BorrowedBook* remove_book_from_vector_borrowed(BorrowedBookVector* vector, size_t index);
void free_borrowed_book_vector(BorrowedBookVector* vector);

// RegisteredUserVector operations
bool add_user_to_vector(const RegisteredUser* user);
RegisteredUser* remove_user_from_vector(size_t index);
void free_user_vector();

#define add_book_to_vector(x, y) _Generic((x), BookVector*: add_book_to_vector_normal, BorrowedBookVector*: add_book_to_vector_borrowed)(x, y)

#define free_book_vector(x) _Generic((x), BookVector*: free_book_vector_normal, BorrowedBookVector*: free_borrowed_book_vector)(x)

#endif  // VECTOR_H
