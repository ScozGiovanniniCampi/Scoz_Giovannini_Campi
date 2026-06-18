#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "book.h"
#include "socket.h"
#include "util.h"
#include "book_loader.h"
#include "operations.h"

void* pthread_run (void* arg) {
    int cfd = (int)arg;
    OperationType op_code = read_operator(cfd);

    char** args;
    size_t* sizes;
    int counter = 0;

    char* tmp;
    int tmp_size;

    while ((tmp_size = read_argument(cfd, &tmp)) > 0) {
        sizes = realloc(sizes, (++counter) * sizeof(size_t));
        sizes[counter - 1] = tmp_size;
        args = realloc(args, (counter) * sizeof(char*));
        args[counter - 1] = tmp;
    }

    free(tmp);
    counter--; // Decrement counter to exclude the last empty string

    sleep(rand() % 5 + 1);

    switch (op_code) {
        case OP_ANSWER:
            // Handle OP_ANSWER
            break;
        case OP_REGISTER:
            // Handle OP_REGISTER
            break;
        case OP_SEARCH:
            // Handle OP_SEARCH
            break;
        case OP_SEARCH_RESULT:
            // Handle OP_SEARCH_RESULT
            break;
        case OP_BORROW:
            // Handle OP_BORROW
            break;
        case OP_RETURN:
            // Handle OP_RETURN
            break;
        case OP_GET_USERS:
            // Handle OP_GET_USERS
            break;
        case OP_USERS_RESULT:
            // Handle OP_USERS_RESULT
            break;
        case OP_GET_BOOKS:
            // Handle OP_GET_BOOKS
            break;
        case OP_BOOKS_RESULT:
            // Handle OP_BOOKS_RESULT
            break;
        default:
            fprintf(stderr, "Unknown operation: %c\n", buffer[0]);
            break;
    }

    lseek(cfd, 1, SEEK_CUR); // Skip the END_OF_TRANSMISSION character for the next read
    for (int i = 0; i < counter; i++) {
        free(args[i]);
    }
    free(args);
    free(sizes);
    close(cfd);
}

void loop(LibrarySocket *sock) {
    while (1) {
        int cfd = accept(sock->fd, NULL, NULL);
        if (cfd < 0) {
            perror("accept");
            continue;
        }
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, pthread_run, (void*)cfd) != 0) {
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

    unsigned int libraryId = atoi(argv[1]);
    unsigned int numTotalLibraries = atoi(argv[2]);
    BookVector *books = loadBooksFromFile(argv[3]);

    LibrarySocket sock;
    if (socket_init_server(&sock, libraryId) < 0) {
        fprintf(stderr, "Failed to initialize server socket\n");
        return 1;
    }

    // loop(&sock);

    socket_close(&sock);

    if (books) {
        free_book_vector(books);
        free(books);
    }

    return 0;
}