#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "network/messages.h"

unsigned int global_library_id = 0;
unsigned int global_num_total_libraries = 0;

size_t read_argument(int socket_fd, char** buffer_out) {
    size_t capacity = 16;
    char* buffer = malloc(capacity);
    if (!buffer) {
        perror("malloc");
        *buffer_out = NULL;
        return 0;
    }

    size_t size = 0;
    char character;
    ssize_t bytesRead;

    while (1) {
        bytesRead = read(socket_fd, &character, 1);
        if (bytesRead <= 0) {
            break;
        }

        if (character == END_OF_ARGUMENT) {
            break;
        }
        if (character == END_OF_TRANSMISSION) {
            lseek(socket_fd, -1, SEEK_CUR);  // Move back to re-read EOT later
            break;
        }

        if (size + 1 >= capacity) {
            capacity *= 2;
            char* new_buf = realloc(buffer, capacity);
            if (!new_buf) {
                perror("realloc");
                free(buffer);
                *buffer_out = NULL;
                return 0;
            }
            buffer = new_buf;
        }
        buffer[size++] = character;
    }

    buffer[size] = '\0';
    *buffer_out = buffer;
    return size;
}

OperationType read_operator(int socket_fd) {
    char* buffer = NULL;
    if (read_argument(socket_fd, &buffer) == 0) {  // No data read, return an error code
        if (buffer) {
            free(buffer);
        }
        return OP_ERROR;  // Indicate an error
    }
    OperationType op_code = strtol(buffer, NULL, 10);
    free(buffer);
    return op_code;
}
