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

void loop(LibrarySocket *sock) {
    while (1) {
        int cfd = accept(sock->fd, NULL, NULL);
        if (cfd < 0) {
            perror("accept");
            continue;
        }
        OperationType op_code = reasock(cfd);
        switch (op_code) {
            case OP_ANSWER:
                char* error = reasock(cfd);
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
            case OP_BORROW_RESULT:
                // Handle OP_BORROW_RESULT
                break;
            case OP_RETURN:
                // Handle OP_RETURN
                break;
            case OP_GET_USERS:
                // Handle OP_GET_USERS
                break;
            case OP_GET_BOOKS:
                // Handle OP_GET_BOOKS
                break;
            default:
                fprintf(stderr, "Unknown operation: %c\n", buffer[0]);
                break;
            }
        }
        if (numRead < 0) {
            perror("read");
        }
        close(cfd);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ID> <books_file.csv>\n", argv[0]);
        return 1;
    }

    unsigned int libraryId = atoi(argv[1]);
    BookVector *books = loadBooksFromFile(argv[2]);

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