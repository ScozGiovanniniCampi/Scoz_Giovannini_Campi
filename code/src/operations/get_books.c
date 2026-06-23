#define _DEFAULT_SOURCE // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE     // NOLINT(bugprone-reserved-identifier)

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/book.h"
#include "models/vector.h"
#include "network/socket.h"
#include "operations.h"
#include "utils/util.h"

void handle_get_books(int socket_fd) {
    printf("[Library %u] Handling get books request\n", global_library_id);

    send_argument(socket_fd, operationType_to_char(OP_BOOKS_RESULT));

    pthread_mutex_lock(&global_book_vector.mutex);
    size_t count = global_book_vector.size;

    char** temp_titles = NULL;
    char** temp_statuses = NULL;

    if (count > 0) {
        temp_titles = (char**)malloc(count * sizeof(char*));
        temp_statuses = (char**)malloc(count * sizeof(char*));
        for (size_t i = 0; i < count; ++i) {
            temp_titles[i] = strdup(global_book_vector.data[i].title);
            if (global_book_vector.data[i].status == AVAILABLE) {
                temp_statuses[i] = strdup("AVAILABLE");
            } else {
                temp_statuses[i] = strdup("BORROWED");
            }
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    send_argument(socket_fd, size_t_to_char(count));

    if (count > 0) {
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_titles[i]);
            send_argument(socket_fd, temp_statuses[i]);
            free(temp_titles[i]);
            free(temp_statuses[i]);
        }
        free((void*)temp_titles);
        free((void*)temp_statuses);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}
