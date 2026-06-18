#ifndef BOOK_H
#define BOOK_H
#include <stddef.h>

#define MAX_TITLE_LENGTH 100
#define MAX_AUTHOR_LENGTH 50

typedef enum {
    AVAILABLE,
    BORROWED
} BookStatus;

typedef struct Book {
    char title[MAX_TITLE_LENGTH];
    char author[MAX_AUTHOR_LENGTH];
    int publicationYear;
    BookStatus status;
} Book;

typedef struct BorrowedBook {
    Book book;
    unsigned int borrowerId;
} BorrowedBook;

typedef struct {
    Book *data;
    size_t size;
    size_t capacity;
} BookVector;

typedef struct {
    BorrowedBook *data;
    size_t size;
    size_t capacity;
} BorrowedBookVector;

#endif // BOOK_H