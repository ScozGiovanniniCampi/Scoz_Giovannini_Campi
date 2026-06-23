#define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE      // NOLINT(bugprone-reserved-identifier)

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/vector.h"
#include "network/socket.h"
#include "operations.h"
#include "utils/util.h"

void handle_get_users(int socket_fd) {
    printf("[Library %u] Handling get users request\n", global_library_id);

    send_argument(socket_fd, operationType_to_char(OP_USERS_RESULT));

    pthread_mutex_lock(&global_user_vector.mutex);
    pthread_mutex_lock(&global_borrowed_book_vector.mutex);

    size_t count = global_user_vector.size;
    char** temp_users = NULL;
    char** temp_books = NULL;

    if (count > 0) {
        temp_users = (char**)malloc(count * sizeof(char*));
        temp_books = (char**)malloc(count * sizeof(char*));
        for (size_t i = 0; i < count; ++i) {
            temp_users[i] = strdup(global_user_vector.data[i].name);
            temp_books[i] = strdup("None");

            if (global_user_vector.data[i].hasBorrowedBook) {
                for (size_t j = 0; j < global_borrowed_book_vector.size; ++j) {
                    if (strcmp(global_borrowed_book_vector.data[j].borrowerId, temp_users[i]) == 0) {
                        free(temp_books[i]);
                        temp_books[i] = strdup(global_borrowed_book_vector.data[j].book.title);
                        break;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&global_borrowed_book_vector.mutex);
    pthread_mutex_unlock(&global_user_vector.mutex);

    send_argument(socket_fd, size_t_to_char(count));

    if (count > 0) {
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_users[i]);
            send_argument(socket_fd, temp_books[i]);
            free(temp_users[i]);
            free(temp_books[i]);
        }
        free((void*)temp_users);
        free((void*)temp_books);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}
