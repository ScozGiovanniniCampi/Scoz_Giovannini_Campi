#ifndef BOOK_H
#define BOOK_H
#include <pthread.h>
#include <stddef.h>

#include "../network/messages.h"

typedef enum { AVAILABLE, BORROWED, NOT_FOUND } BookStatus;

typedef struct Book {
    char* title;
    char* author;
    int publicationYear;
    BookStatus status;
} Book;

typedef struct BorrowedBook {
    Book book;
    char* borrowerId;
    UserType borrowerType;
    int ownerLibraryId;
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