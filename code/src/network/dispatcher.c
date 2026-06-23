#include "dispatcher.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/book.h"
#include "models/vector.h"
#include "network/socket.h"
#include "operations/operations.h"
#include "utils/book_loader.h"
#include "utils/util.h"

static int is_counter_valid(OperationType op_code, int counter, char** args) {
    switch (op_code) {
        case OP_ANSWER:
        case OP_REGISTER:
            return counter == 1;
        case OP_SEARCH:
        case OP_BORROW:
        case OP_RETURN:
            return counter == 3;
        case OP_GET_USERS:
        case OP_GET_BOOKS:
            return counter == 0;
        case OP_SEARCH_RESULT:
        case OP_USERS_RESULT:
        case OP_BOOKS_RESULT: {
            if (counter < 1) {
                return 0;
            }
            int count = (int)strtol(args[0], NULL, 10);
            return counter == 1 + count;
        }
        default:
            return 1;
    }
}

static void dispatch_operation(int cfd, OperationType op_code, char** args) {
    switch (op_code) {
        case OP_ANSWER:
            printf("[Library %u] Received answer: result_code=%d\n", global_library_id, char_to_resultCode(args[0]));
            break;
        case OP_REGISTER:
            handle_register(cfd, args[0]);
            break;
        case OP_SEARCH:
            handle_search(cfd, char_to_userType(args[0]), char_to_searchType(args[1]), args[2]);
            break;
        case OP_SEARCH_RESULT: {
            int count = (int)strtol(args[0], NULL, 10);
            printf("[Library %u] Received search result: book_count=%d\n", global_library_id, count);
            break;
        }
        case OP_BORROW:
            handle_borrow(cfd, char_to_userType(args[0]), args[1], args[2]);
            break;
        case OP_RETURN:
            handle_return(cfd, char_to_userType(args[0]), args[1], args[2]);
            break;
        case OP_GET_USERS:
            handle_get_users(cfd);
            break;
        case OP_USERS_RESULT: {
            int count = (int)strtol(args[0], NULL, 10);
            printf("[Library %u] Received users result: user_count=%d\n", global_library_id, count);
            break;
        }
        case OP_GET_BOOKS:
            handle_get_books(cfd);
            break;
        case OP_BOOKS_RESULT: {
            int count = (int)strtol(args[0], NULL, 10);
            printf("[Library %u] Received books result: book_count=%d\n", global_library_id, count);
            break;
        }
        default:
            fprintf(stderr, "Unknown operation: %d\n", op_code);
            break;
    }
}

static void* pthread_run(void* arg) {
    int cfd = (int)(intptr_t)arg;

    char** args = NULL;
    size_t* sizes = NULL;
    int counter = 0;
    OperationType op_code = fetch_arguments(cfd, &args, &sizes, &counter);
    if (op_code == OP_ERROR || counter < 0 || (counter > 0 && args == NULL)) {
        free((void*)args);
        free(sizes);
        close(cfd);
        return NULL;
    }

    sleep((rand() % 5) + 1);

    if (is_counter_valid(op_code, counter, args)) {
        dispatch_operation(cfd, op_code, args);
    }

    // Skip the END_OF_TRANSMISSION character for the next read
    char eot_check;
    if (read(cfd, &eot_check, 1) > 0 && eot_check != END_OF_TRANSMISSION) {
        lseek(cfd, -1, SEEK_CUR);
    }

    for (int i = 0; i < counter; i++) {
        free(args[i]);
    }
    free((void*)args);
    free(sizes);
    close(cfd);
    return NULL;
}

void start_server_loop(LibrarySocket* sock) {
    while (1) {
        int cfd = accept(sock->fd, NULL, NULL);
        if (cfd < 0) {
            perror("accept");
            continue;
        }
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, pthread_run, (void*)(intptr_t)cfd) != 0) {
            perror("pthread_create");
            close(cfd);
        } else {
            pthread_detach(thread_id);  // Detach the thread to clean up resources when it finishes
        }
    }
}
