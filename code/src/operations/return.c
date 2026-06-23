/* Expose POSIX interface strdup in string.h. Suppress warning because POSIX macro names begin with a reserved underscore. */
#define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE      // NOLINT(bugprone-reserved-identifier)

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

static ResultCode return_to_specific_library(const char* book_title, int target_library_id) {
    int peer_fd = socket_connect_to_server(target_library_id);
    if (peer_fd < 0) {
        return ERROR_BOOK_NOT_FOUND;
    }

    send_argument(peer_fd, operationType_to_char(OP_RETURN));
    send_argument(peer_fd, userType_to_char(USER_LIBRARY));
    send_argument(peer_fd, size_t_to_char((size_t)global_library_id));
    send_argument(peer_fd, book_title);
    write(peer_fd, &(char){END_OF_TRANSMISSION}, 1);

    ResultCode resCode = get_borrow_response(peer_fd, target_library_id, book_title);
    close(peer_fd);
    return resCode;
}

static ResultCode validate_returning_user(const char* sender_id) {
    ResultCode user_check = ERROR_USER_NOT_REGISTERED;
    bool user_has_borrowed = false;
    pthread_mutex_lock(&global_user_vector.mutex);
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, sender_id) == 0) {
            user_has_borrowed = global_user_vector.data[i].hasBorrowedBook;
            user_check = RESULT_SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);

    if (user_check != RESULT_SUCCESS) {
        return user_check;
    }
    if (!user_has_borrowed) {
        return ERROR_USER_HAS_NO_BORROWED_BOOK;
    }
    return RESULT_SUCCESS;
}

static bool check_borrowed_specific_book(const char* sender_id, UserType user_type, const char* book_title, int* target_library_id_out) {
    bool borrowed_this_book = false;
    pthread_mutex_lock(&global_borrowed_book_vector.mutex);
    for (size_t i = 0; i < global_borrowed_book_vector.size; ++i) {
        if (strcmp(global_borrowed_book_vector.data[i].borrowerId, sender_id) == 0 && global_borrowed_book_vector.data[i].borrowerType == user_type &&
            strcmp(global_borrowed_book_vector.data[i].book.title, book_title) == 0) {
            borrowed_this_book = true;
            if (target_library_id_out) {
                *target_library_id_out = global_borrowed_book_vector.data[i].ownerLibraryId;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_borrowed_book_vector.mutex);
    return borrowed_this_book;
}

static ResultCode try_return_local_book(const char* book_title, bool* belongs_to_us_out) {
    ResultCode res_code = ERROR_BOOK_NOT_BORROWED;
    bool belongs_to_us = false;
    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            belongs_to_us = true;
            if (book->status == BORROWED) {
                book->status = AVAILABLE;
                res_code = RESULT_SUCCESS;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);
    if (belongs_to_us_out) {
        *belongs_to_us_out = belongs_to_us;
    }
    return belongs_to_us ? res_code : ERROR_BOOK_NOT_FOUND;
}

static void cleanup_borrow_record(const char* sender_id, UserType user_type, const char* book_title) {
    pthread_mutex_lock(&global_borrowed_book_vector.mutex);
    for (size_t i = 0; i < global_borrowed_book_vector.size; ++i) {
        if (strcmp(global_borrowed_book_vector.data[i].borrowerId, sender_id) == 0 && global_borrowed_book_vector.data[i].borrowerType == user_type &&
            strcmp(global_borrowed_book_vector.data[i].book.title, book_title) == 0) {
            BorrowedBook* removed = remove_book_from_vector_borrowed(&global_borrowed_book_vector, i);
            free(removed->book.title);
            free(removed->borrowerId);
            free(removed);
            break;
        }
    }
    pthread_mutex_unlock(&global_borrowed_book_vector.mutex);

    set_user_borrow_status(sender_id, false);
}

void handle_return(int socket_fd, UserType user_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling return request: user_type=%d, sender_id=%s, book_title=%s\n", global_library_id, user_type, sender_id, book_title);

    ResultCode res_code = RESULT_FAILURE;
    bool belongs_to_us = false;
    int target_library_id = -1;

    if (user_type == USER_USER) {
        // 1. Check if user is registered and has borrowed a book
        res_code = validate_returning_user(sender_id);
        if (res_code != RESULT_SUCCESS) {
            send_argument(socket_fd, operationType_to_char(OP_ANSWER));
            send_argument(socket_fd, resultCode_to_char(res_code));
            write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
            return;
        }

        // 2. Check if the user borrowed this specific book
        if (!check_borrowed_specific_book(sender_id, user_type, book_title, &target_library_id)) {
            send_argument(socket_fd, operationType_to_char(OP_ANSWER));
            send_argument(socket_fd, resultCode_to_char(ERROR_BOOK_NOT_BORROWED_BY_USER));
            write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
            return;
        }
    }

    // 3. Try return locally
    res_code = try_return_local_book(book_title, &belongs_to_us);

    // 4. Try return remotely if needed
    if (!belongs_to_us && user_type == USER_USER) {
        res_code = (target_library_id != -1) ? return_to_specific_library(book_title, target_library_id) : ERROR_BOOK_NOT_FOUND;
    }

    // 5. Clean up borrow record on success
    if (res_code == RESULT_SUCCESS && user_type == USER_USER) {
        cleanup_borrow_record(sender_id, user_type, book_title);
    }

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));
    send_argument(socket_fd, resultCode_to_char(res_code));
    write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
}
