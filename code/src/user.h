#ifndef USER_H
#define USER_H
#include <stddef.h>
#include <pthread.h>

#define MAX_USER_LENGTH 50

typedef struct RegisteredUser {
    char name[MAX_USER_LENGTH];
    bool hasBorrowedBook;
} RegisteredUser;

typedef struct {
    RegisteredUser *data;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex; // Mutex for thread-safe access
} RegisteredUserVector;

#endif // USER_H