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


OperationType fetch_arguments(int cfd, char*** args_out, size_t** sizes_out, int* counter_out) {
    OperationType op_code = read_operator(cfd);

    char** args = NULL;
    size_t* sizes = NULL;
    int counter = 0;

    char* tmp = NULL;
    size_t tmp_size;

    while (1) {
        tmp_size = read_argument(cfd, &tmp);
        if (tmp_size == 0) {
            free(tmp);
            break;
        }

        size_t* new_sizes = (size_t*)realloc(sizes, (counter + 1) * sizeof(size_t));
        if (!new_sizes) {
            perror("realloc sizes");
            free(tmp);
            goto cleanup;
        }
        sizes = new_sizes;
        sizes[counter] = tmp_size;

        char** new_args = realloc(args, (counter + 1) * sizeof(char*));
        if (!new_args) {
            perror("realloc args");
            free(tmp);
            goto cleanup;
        }
        args = new_args;
        args[counter] = tmp;
        counter++;
    }

    *args_out = args;
    *sizes_out = sizes;
    *counter_out = counter;
    return op_code;

cleanup:
    for (int i = 0; i < counter; ++i) {
        free(args[i]);
    }
    free(args);
    free(sizes);
    *args_out = NULL;
    *sizes_out = NULL;
    *counter_out = 0;
    return op_code;
}
