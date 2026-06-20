#ifndef BOOK_H
#define BOOK_H
#include <pthread.h>
#include <stddef.h>

#define MAX_TITLE_LENGTH 100
#define MAX_AUTHOR_LENGTH 50
#define MAX_BORROWER_LENGTH 50

typedef enum { AVAILABLE, BORROWED, NOT_FOUND } BookStatus;

// TODO: switch to heap save memory and avoid fixed size arrays for title and author
typedef struct Book {
    char title[MAX_TITLE_LENGTH];
    char author[MAX_AUTHOR_LENGTH];
    int publicationYear;
    BookStatus status;
} Book;

typedef struct BorrowedBook {
    Book book;
    char borrowerId[MAX_BORROWER_LENGTH];
} BorrowedBook;

typedef struct {
    Book* data;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex;  // Mutex for thread-safe access
} BookVector;

typedef struct {
    BorrowedBook* data;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex;  // Mutex for thread-safe access
} BorrowedBookVector;

#endif  // BOOK_H