#include "operations.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/vector.h"
#include "network/messages.h"
#include "network/socket.h"
#include "utils/util.h"

void handle_answer(int socket_fd, requestId reqId, ResultCode result_code) {
    (void)socket_fd;
    (void)reqId;
    (void)result_code;
}

void handle_register(int socket_fd, requestId reqId, const char* username) {
    printf("[Library %u] Handling register request: reqId=%d, username=%s\n", global_library_id, reqId, username);

    RegisteredUser user;
    memset(&user, 0, sizeof(user));
    strncpy(user.name, username, MAX_USER_LENGTH - 1);
    user.name[MAX_USER_LENGTH - 1] = '\0';
    user.hasBorrowedBook = false;

    pthread_mutex_lock(&global_user_vector.mutex);
    bool success = add_user_to_vector(&user);
    pthread_mutex_unlock(&global_user_vector.mutex);

    if (!success) {
        send_argument(socket_fd, operationType_to_char(OP_ANSWER));
        send_argument(socket_fd, reqId_to_char(reqId));
        send_argument(socket_fd, resultCode_to_char(RESULT_FAILURE));
    } else {
        send_argument(socket_fd, operationType_to_char(OP_ANSWER));
        send_argument(socket_fd, reqId_to_char(reqId));
        send_argument(socket_fd, resultCode_to_char(RESULT_SUCCESS));
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}

void handle_search(int socket_fd, requestId reqId, SearchType search_type, const char* search_term) {
    (void)socket_fd;
    (void)reqId;
    (void)search_type;
    (void)search_term;
}

void handle_search_result(int socket_fd, requestId reqId, int book_count, const char** books) {
    (void)socket_fd;
    (void)reqId;
    (void)book_count;
    (void)books;
}

static void borrow_from_remote_libraries(requestId reqId, const char* sender_id, const char* book_title) {
    for (size_t i = 1; i <= global_num_total_libraries; ++i) {
        if (i == global_library_id) {
            continue;  // Skip the current library
        }
        int peer_fd = socket_connect_to_server((int)i);
        if (peer_fd < 0) {
            fprintf(stderr, "Failed to connect to library %zu\n", i);
            continue;
        }

        send_argument(peer_fd, operationType_to_char(OP_BORROW));
        send_argument(peer_fd, reqId_to_char(reqId));
        send_argument(peer_fd, senderType_to_char(SENDER_LIBRARY));
        send_argument(peer_fd, sender_id);
        send_argument(peer_fd, book_title);

        char eot_check;
        if (read(peer_fd, &eot_check, 1) > 0 && eot_check != END_OF_TRANSMISSION) {
            lseek(peer_fd, -1, SEEK_CUR);
        }

        close(peer_fd);
    }
}

void handle_borrow(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling borrow request: reqId=%d, sender_type=%d, sender_id=%s, book_title=%s\n", global_library_id, reqId, sender_type, sender_id, book_title);

    if (sender_type == SENDER_USER) {
        send_argument(socket_fd, operationType_to_char(OP_ANSWER));  // Operation type
        send_argument(socket_fd, reqId_to_char(reqId));              // reqId

        bool book_found = false;
        ResultCode res_code = RESULT_SUCCESS;

        pthread_mutex_lock(&global_book_vector.mutex);
        for (size_t i = 0; i < global_book_vector.size; ++i) {
            Book* book = &global_book_vector.data[i];
            if (strcmp(book->title, book_title) == 0) {
                if (book->status == AVAILABLE) {
                    book->status = BORROWED;
                    res_code = RESULT_SUCCESS;
                } else {
                    res_code = ERROR_BOOK_ALREADY_BORROWED;
                }
                book_found = true;
                break;
            }
        }
        pthread_mutex_unlock(&global_book_vector.mutex);

        if (book_found) {
            send_argument(socket_fd, resultCode_to_char(res_code));  // Result code
        } else {
            borrow_from_remote_libraries(reqId, sender_id, book_title);
        }
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);  // Signal end of transmission
}

void handle_return(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling return request: reqId=%d, sender_type=%d, sender_id=%s, book_title=%s\n", global_library_id, reqId, sender_type, sender_id, book_title);

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));  // Operation type
    send_argument(socket_fd, reqId_to_char(reqId));              // reqId

    bool book_found = false;
    ResultCode res_code = RESULT_FAILURE;

    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            if (book->status == BORROWED) {
                book->status = AVAILABLE;
                res_code = RESULT_SUCCESS;
            } else {
                res_code = RESULT_FAILURE;
            }
            book_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    if (book_found) {
        send_argument(socket_fd, resultCode_to_char(res_code));  // Result code
    } else {
        send_argument(socket_fd, resultCode_to_char(RESULT_FAILURE));  // Result code
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);  // Signal end of transmission
}

void handle_get_users(int socket_fd, requestId reqId) {
    printf("[Library %u] Handling get users request: reqId=%d\n", global_library_id, reqId);

    send_argument(socket_fd, operationType_to_char(OP_USERS_RESULT));  // Operation type
    send_argument(socket_fd, reqId_to_char(reqId));                    // reqId

    pthread_mutex_lock(&global_user_vector.mutex);
    size_t count = global_user_vector.size;
    char (*temp_users)[MAX_USER_LENGTH] = NULL;
    if (count > 0) {
        temp_users = malloc(count * sizeof(*temp_users));
        if (temp_users) {
            for (size_t i = 0; i < count; ++i) {
                strcpy(temp_users[i], global_user_vector.data[i].name);
            }
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);

    if (count > 0 && !temp_users) {
        send_argument(socket_fd, size_t_to_char(0));
    } else {
        send_argument(socket_fd, size_t_to_char(count));
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_users[i]);
        }
        free(temp_users);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}

void handle_users_result(int socket_fd, requestId reqId, int user_count, const char** users) {
    (void)socket_fd;
    (void)reqId;
    (void)user_count;
    (void)users;
}

void handle_get_books(int socket_fd, requestId reqId) {
    printf("[Library %u] Handling get books request: reqId=%d\n", global_library_id, reqId);

    pthread_mutex_lock(&global_book_vector.mutex);
    size_t count = global_book_vector.size;
    const char** temp_titles = NULL;
    if (count > 0) {
        temp_titles = malloc(count * sizeof(char*));
        if (temp_titles) {
            for (size_t i = 0; i < count; ++i) {
                temp_titles[i] = global_book_vector.data[i].title;
            }
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    if (temp_titles) {
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_titles[i]);
        }
        free(temp_titles);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}

void handle_books_result(int socket_fd, requestId reqId, int book_count, const char** books) {
    (void)socket_fd;
    (void)reqId;
    (void)book_count;
    (void)books;
}