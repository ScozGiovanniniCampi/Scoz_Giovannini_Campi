#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include "book.h"
#include "socket.h"
#include "util.h"
#include "book_loader.h"
#include "operations.h"

void* pthread_run (void* arg) {
    int cfd = (int)(intptr_t)arg;
    OperationType op_code = read_operator(cfd);

    char** args = NULL;
    size_t* sizes = NULL;
    int counter = 0;

    char* tmp = NULL;
    int tmp_size;

    while ((tmp_size = read_argument(cfd, &tmp)) > 0) {
        sizes = realloc(sizes, (++counter) * sizeof(size_t));
        if (!sizes) {
            perror("realloc sizes");
            free(tmp);
            break;
        }
        sizes[counter - 1] = tmp_size;
        args = realloc(args, (counter) * sizeof(char*));
        if (!args) {
            perror("realloc args");
            free(tmp);
            break;
        }
        args[counter - 1] = tmp;
    }


    // Free tmp if read_argument allocated it but returned 0 or error
    if (tmp_size <= 0) {
        free(tmp);
    }

    sleep(rand() % 5 + 1);

    switch (op_code) {
        case OP_ANSWER:
            // Handle OP_ANSWER
            handle_answer(cfd, char_to_reqId(args[0]), char_to_resultCode(args[1]));
            break;
        case OP_REGISTER:
            // Handle OP_REGISTER
            handle_register(cfd, char_to_reqId(args[0]), args[1]);
            break;
        case OP_SEARCH:
            // Handle OP_SEARCH
            handle_search(cfd, char_to_reqId(args[0]), char_to_searchType(args[1]), args[2]);
            break;
        case OP_SEARCH_RESULT:
            // Handle OP_SEARCH_RESULT
            handle_search_result(cfd, char_to_reqId(args[0]), atoi(args[1]), (const char**)&args[2]);
            break;
        case OP_BORROW:
            // Handle OP_BORROW
            handle_borrow(cfd, char_to_reqId(args[0]), char_to_senderType(args[1]), args[2], args[3]);
            break;
        case OP_RETURN:
            // Handle OP_RETURN
            handle_return(cfd, char_to_reqId(args[0]), char_to_senderType(args[1]), args[2], args[3]);
            break;
        case OP_GET_USERS:
            // Handle OP_GET_USERS
            handle_get_users(cfd, char_to_reqId(args[0]));
            break;
        case OP_USERS_RESULT:
            // Handle OP_USERS_RESULT
            handle_users_result(cfd, char_to_reqId(args[0]), atoi(args[1]), (const char**)&args[2]);
            break;
        case OP_GET_BOOKS:
            // Handle OP_GET_BOOKS
            handle_get_books(cfd, char_to_reqId(args[0]));
            break;
        case OP_BOOKS_RESULT:
            // Handle OP_BOOKS_RESULT
            handle_books_result(cfd, char_to_reqId(args[0]), atoi(args[1]), (const char**)&args[2]);
            break;
        default:
            fprintf(stderr, "Unknown operation: %d\n", op_code);
            break;
    }

    // Skip the END_OF_TRANSMISSION character for the next read
    char eot_check;
    if (read(cfd, &eot_check, 1) > 0 && eot_check != END_OF_TRANSMISSION) {
        lseek(cfd, -1, SEEK_CUR);
    }

    for (int i = 0; i < counter; i++) {
        free(args[i]);
    }
    free(args);
    free(sizes);
    close(cfd);
    return NULL;
}

void loop(LibrarySocket *sock) {
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
            pthread_detach(thread_id); // Detach the thread to clean up resources when it finishes
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <library_id> <num_total_libraries> <catalog_file.csv>\n", argv[0]);
        return 1;
    }

    srand(time(NULL)); // Initialize random seed

    global_library_id = atoi(argv[1]);
    global_num_total_libraries = atoi(argv[2]);

    // Initialize global vectors
    global_book_vector = loadBooksFromFile(argv[3]);
    if (!global_book_vector) {
        fprintf(stderr, "Failed to load books from file: %s\n", argv[3]);
        return 1;
    }

    global_borrowed_book_vector = calloc(1, sizeof(BorrowedBookVector));
    if (global_borrowed_book_vector) {
        pthread_mutex_init(&global_borrowed_book_vector->mutex, NULL);
    }

    global_user_vector = calloc(1, sizeof(RegisteredUserVector));
    if (global_user_vector) {
        pthread_mutex_init(&global_user_vector->mutex, NULL);
    }

    LibrarySocket sock;
    if (socket_init_server(&sock, global_library_id) < 0) {
        fprintf(stderr, "Failed to initialize server socket\n");
        return 1;
    }

    // loop(&sock);

    socket_close(&sock);

    if (global_book_vector) {
        free_book_vector(global_book_vector);
        free(global_book_vector);
        global_book_vector = NULL;
    }

    if (global_borrowed_book_vector) {
        free_borrowed_book_vector(global_borrowed_book_vector);
        free(global_borrowed_book_vector);
        global_borrowed_book_vector = NULL;
    }

    if (global_user_vector) {
        free_user_vector();
        free(global_user_vector);
        global_user_vector = NULL;
    }

    return 0;
}