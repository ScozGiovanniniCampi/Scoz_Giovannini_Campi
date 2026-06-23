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

static void cleanup(char*** peer_args, size_t** peer_sizes, size_t counter) {
    if (*peer_args) {
        for (size_t i = 0; i < counter; ++i) {
            free((*peer_args)[i]);
        }
        free((void*)*peer_args);
    }
    free(*peer_sizes);
}

static ResultCode get_borrow_response(int peer_fd, int library_id, const char* book_title) {
    (void)book_title;
    char** peer_args = NULL;
    size_t* peer_sizes = NULL;
    int counter = 0;
    OperationType peer_op_code = fetch_arguments(peer_fd, &peer_args, &peer_sizes, &counter);
    ResultCode res_code = ERROR_BOOK_NOT_FOUND;

    if (peer_op_code != OP_ANSWER || peer_args == NULL || counter < 1) {
        if (peer_op_code != OP_ANSWER) {
            fprintf(stderr, "Unexpected operation code from library %d: %d\n", library_id, peer_op_code);
        } else {
            fprintf(stderr, "Invalid borrow response from library %d: counter=%d\n", library_id, counter);
        }
        cleanup(&peer_args, &peer_sizes, counter);
        return res_code;
    }

    res_code = char_to_resultCode(peer_args[0]);
    cleanup(&peer_args, &peer_sizes, counter);
    return res_code;
}

typedef struct {
    const char* book_title;
    unsigned int peer_library_id;
    ResultCode result_code;
} BorrowThreadArg;

static void* borrow_remote_library_thread(void* arg) {
    BorrowThreadArg* borrow_arg = (BorrowThreadArg*)arg;
    int peer_fd = socket_connect_to_server((int)borrow_arg->peer_library_id);
    if (peer_fd < 0) {
        fprintf(stderr, "Failed to connect to library %d\n", borrow_arg->peer_library_id);
        borrow_arg->result_code = ERROR_BOOK_NOT_FOUND;
        return NULL;
    }

    send_argument(peer_fd, operationType_to_char(OP_BORROW));
    send_argument(peer_fd, userType_to_char(USER_LIBRARY));
    send_argument(peer_fd, unsigned_int_to_char(global_library_id));
    send_argument(peer_fd, borrow_arg->book_title);
    write(peer_fd, &(char){END_OF_TRANSMISSION}, 1);  // Signal end of transmission

    borrow_arg->result_code = get_borrow_response(peer_fd, (int)borrow_arg->peer_library_id, borrow_arg->book_title);
    close(peer_fd);
    return NULL;
}

static ResultCode borrow_from_remote_libraries(const char* book_title, int* library_id_out) {
    unsigned int num_threads = 0;
    if (global_num_total_libraries > 1) {
        num_threads = global_num_total_libraries - 1;
    }

    if (num_threads == 0) {
        return ERROR_BOOK_NOT_FOUND;
    }

    pthread_t* threads = calloc(num_threads, sizeof(pthread_t));
    BorrowThreadArg* args = calloc(num_threads, sizeof(BorrowThreadArg));
    if (!threads || !args) {
        perror("calloc");
        free(threads);
        free(args);
        return ERROR_BOOK_NOT_FOUND;
    }

    unsigned int thread_index = 0;
    for (unsigned int i = 0; i < global_num_total_libraries; ++i) {
        if (i == global_library_id) {
            continue;  // Skip the current library
        }

        args[thread_index].book_title = book_title;
        args[thread_index].peer_library_id = i;
        args[thread_index].result_code = ERROR_BOOK_NOT_FOUND;

        if (pthread_create(&threads[thread_index], NULL, borrow_remote_library_thread, &args[thread_index]) != 0) {
            perror("pthread_create");
            args[thread_index].result_code = ERROR_BOOK_NOT_FOUND;
        } else {
            thread_index++;
        }
    }

    unsigned int actual_threads = thread_index;
    ResultCode final_result = ERROR_BOOK_NOT_FOUND;
    int success_library_id = -1;

    for (unsigned int i = 0; i < actual_threads; ++i) {
        pthread_join(threads[i], NULL);
        if (args[i].result_code == RESULT_SUCCESS || args[i].result_code == ERROR_BOOK_ALREADY_BORROWED) {
            final_result = args[i].result_code;
            success_library_id = (int)args[i].peer_library_id;
        }
    }

    free(threads);
    free(args);

    if (success_library_id != -1) {
        *library_id_out = success_library_id;
    }
    return final_result;
}

static ResultCode check_user_can_borrow(const char* username) {
    ResultCode code = ERROR_USER_NOT_REGISTERED;
    pthread_mutex_lock(&global_user_vector.mutex);
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, username) == 0) {
            if (global_user_vector.data[i].hasBorrowedBook) {
                code = ERROR_USER_ALREADY_BORROWED_BOOK;
            } else {
                code = RESULT_SUCCESS;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);
    return code;
}

static void set_user_borrow_status(const char* username, bool status) {
    pthread_mutex_lock(&global_user_vector.mutex);
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, username) == 0) {
            global_user_vector.data[i].hasBorrowedBook = status;
            break;
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);
}

void handle_borrow(int socket_fd, UserType user_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling borrow request: user_type=%d, sender_id=%s, book_title=%s\n", global_library_id, user_type, sender_id, book_title);

    ResultCode res_code = RESULT_SUCCESS;
    bool book_found_and_available = false;

    if (user_type == USER_USER) {
        res_code = check_user_can_borrow(sender_id);
        if (res_code != RESULT_SUCCESS) {
            send_argument(socket_fd, operationType_to_char(OP_ANSWER));
            send_argument(socket_fd, resultCode_to_char(res_code));
            write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
            return;
        }
    }

    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            if (book->status == AVAILABLE) {
                book->status = BORROWED;
                res_code = RESULT_SUCCESS;
                book_found_and_available = true;
            } else {
                res_code = ERROR_BOOK_ALREADY_BORROWED;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    int owner_lib_id = (int)global_library_id;
    if (!book_found_and_available && res_code != ERROR_BOOK_ALREADY_BORROWED && user_type == USER_USER) {
        int library_id = -1;
        res_code = borrow_from_remote_libraries(book_title, &library_id);
        if (res_code == RESULT_SUCCESS) {
            book_found_and_available = true;
            owner_lib_id = library_id;
        }
    } else if (!book_found_and_available && res_code != ERROR_BOOK_ALREADY_BORROWED) {
        res_code = ERROR_BOOK_NOT_FOUND;
    }

    if (book_found_and_available && user_type == USER_USER) {
        BorrowedBook record;
        memset(&record, 0, sizeof(record));
        record.book.title = strdup(book_title);
        record.borrowerId = strdup(sender_id);
        record.borrowerType = user_type;
        record.ownerLibraryId = owner_lib_id;

        pthread_mutex_lock(&global_borrowed_book_vector.mutex);
        add_book_to_vector(&global_borrowed_book_vector, &record);
        pthread_mutex_unlock(&global_borrowed_book_vector.mutex);

        set_user_borrow_status(sender_id, true);
    }

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));
    send_argument(socket_fd, resultCode_to_char(res_code));
    write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
}
