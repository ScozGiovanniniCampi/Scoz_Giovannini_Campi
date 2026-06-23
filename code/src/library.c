#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/book.h"
#include "models/vector.h"
#include "network/dispatcher.h"
#include "network/socket.h"
#include "operations/operations.h"
#include "utils/book_loader.h"
#include "utils/util.h"

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

    start_server_loop(&sock);

    socket_close(&sock);

    free_book_vector(&global_book_vector);
    free_book_vector(&global_borrowed_book_vector);
    free_user_vector();

    return 0;
}