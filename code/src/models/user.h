#ifndef USER_H
#define USER_H
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct RegisteredUser {
    char* name;
    bool hasBorrowedBook;
} RegisteredUser;

typedef struct {
    RegisteredUser* data;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex;  // Mutex for thread-safe access
} RegisteredUserVector;

#endif  // USER_H