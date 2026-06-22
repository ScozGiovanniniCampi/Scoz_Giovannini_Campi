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
            return counter == 2;
        case OP_SEARCH:
        case OP_BORROW:
        case OP_RETURN:
            return counter == 4;
        case OP_GET_USERS:
        case OP_GET_BOOKS:
            return counter == 1;
        case OP_SEARCH_RESULT:
        case OP_USERS_RESULT:
        case OP_BOOKS_RESULT: {
            if (counter < 2) {
                return 0;
            }
            int count = (int)strtol(args[1], NULL, 10);
            return counter == 2 + count;
        }
        default:
            return 1;
    }
}

static void dispatch_operation(int cfd, requestId reqId, OperationType op_code, char** args) {
    switch (op_code) {
        case OP_ANSWER:
            printf("[Library %u] Received answer: reqId=%u, result_code=%d\n", global_library_id, reqId, char_to_resultCode(args[1]));
            break;
        case OP_REGISTER:
            handle_register(cfd, reqId, args[1]);
            break;
        case OP_SEARCH:
            handle_search(cfd, reqId, char_to_userType(args[1]), char_to_searchType(args[2]), args[3]);
            break;
        case OP_SEARCH_RESULT: {
            int count = (int)strtol(args[1], NULL, 10);
            printf("[Library %u] Received search result: reqId=%u, book_count=%d\n", global_library_id, reqId, count);
            break;
        }
        case OP_BORROW:
            handle_borrow(cfd, reqId, char_to_userType(args[1]), args[2], args[3]);
            break;
        case OP_RETURN:
            handle_return(cfd, reqId, char_to_userType(args[1]), args[2], args[3]);
            break;
        case OP_GET_USERS:
            handle_get_users(cfd, reqId);
            break;
        case OP_USERS_RESULT: {
            int count = (int)strtol(args[1], NULL, 10);
            printf("[Library %u] Received users result: reqId=%u, user_count=%d\n", global_library_id, reqId, count);
            break;
        }
        case OP_GET_BOOKS:
            handle_get_books(cfd, reqId);
            break;
        case OP_BOOKS_RESULT: {
            int count = (int)strtol(args[1], NULL, 10);
            printf("[Library %u] Received books result: reqId=%u, book_count=%d\n", global_library_id, reqId, count);
            break;
        }
        default:
            fprintf(stderr, "Unknown operation: %d\n", op_code);
            break;
    }
}

void* pthread_run(void* arg) {
    int cfd = (int)(intptr_t)arg;

    char** args = NULL;
    size_t* sizes = NULL;
    int counter = 0;
    OperationType op_code = fetch_arguments(cfd, &args, &sizes, &counter);
    if (args == NULL || counter < 1) {
        free((void*)args);
        free(sizes);
        close(cfd);
        return NULL;
    }
    requestId reqId = char_to_reqId(args[0]);

    sleep((rand() % 5) + 1);

    if (is_counter_valid(op_code, counter, args)) {
        dispatch_operation(cfd, reqId, op_code, args);
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

void loop(LibrarySocket* sock) {
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

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <library_id> <num_total_libraries> <catalog_file.csv>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));  // Initialize random seed

    global_library_id = (unsigned int)strtol(argv[1], NULL, 10);
    global_num_total_libraries = (unsigned int)strtol(argv[2], NULL, 10);

    // Initialize global vectors
    global_book_vector = loadBooksFromFile(argv[3]);
    if (global_book_vector.data == NULL) {
        fprintf(stderr, "Failed to load books from file: %s\n", argv[3]);
        return 1;
    }

    pthread_mutex_init(&global_borrowed_book_vector.mutex, NULL);
    pthread_mutex_init(&global_user_vector.mutex, NULL);

    LibrarySocket sock;
    if (socket_init_server(&sock, (int)global_library_id) < 0) {
        fprintf(stderr, "Failed to initialize server socket\n");
        return 1;
    }

    loop(&sock);

    socket_close(&sock);

    free_book_vector(&global_book_vector);
    free_book_vector(&global_borrowed_book_vector);
    free_user_vector();

    return 0;
}