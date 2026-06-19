#include "operations.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "messages.h"
#include "socket.h"
#include "util.h"

void handle_answer(int fd, requestId reqId, ResultCode result_code) {
    (void)fd;
    (void)reqId;
    (void)result_code;
}

void handle_register(int fd, requestId reqId, const char* username) {
    printf("Handling register request: reqId=%d, username=%s\n", reqId, username);

    RegisteredUser user;
    memset(&user, 0, sizeof(user));
    strncpy(user.name, username, MAX_USER_LENGTH - 1);
    user.name[MAX_USER_LENGTH - 1] = '\0';
    user.hasBorrowedBook = false;

    pthread_mutex_lock(&global_user_vector.mutex);

    bool success = add_user_to_vector(&user);

    if (!success) {
        send_argument(fd, operationType_to_char(OP_ANSWER));
        send_argument(fd, reqId_to_char(reqId));
        send_argument(fd, resultCode_to_char(RESULT_FAILURE));
    } else {
        send_argument(fd, operationType_to_char(OP_ANSWER));
        send_argument(fd, reqId_to_char(reqId));
        send_argument(fd, resultCode_to_char(RESULT_SUCCESS));
    }

    pthread_mutex_unlock(&global_user_vector.mutex);
}

void handle_search(int fd, requestId reqId, SearchType search_type, const char* search_term) {
    (void)fd;
    (void)reqId;
    (void)search_type;
    (void)search_term;
}

void handle_search_result(int fd, requestId reqId, int book_count, const char** books) {
    (void)fd;
    (void)reqId;
    (void)book_count;
    (void)books;
}

void handle_borrow(int fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    printf("Handling borrow request: reqId=%d, sender_type=%d, sender_id=%s, book_title=%s\n", reqId, sender_type, sender_id, book_title);


    if (sender_type == SENDER_USER) {
        send_argument(fd, operationType_to_char(OP_ANSWER));  // Operation type
        send_argument(fd, reqId_to_char(reqId));              // reqId

        pthread_mutex_lock(&global_book_vector.mutex);

        bool book_found = false;
        for (size_t i = 0; i < global_book_vector.size; ++i) {
            Book* book = &global_book_vector.data[i];
            if (strcmp(book->title, book_title) == 0) {
                if (book->status == AVAILABLE) {
                    book->status = BORROWED;
                    send_argument(fd, resultCode_to_char(RESULT_SUCCESS));  // Result code
                } else {
                    send_argument(fd, resultCode_to_char(ERROR_BOOK_ALREADY_BORROWED));  // Result code
                }
                book_found = true;
                break;
            }
        }
        if (!book_found) {
            for (size_t i = 0; i < global_num_total_libraries; ++i) {
                if (i + 1 == global_library_id) {
                    continue;  // Skip the current library
                }
                int peer_fd = socket_connect_to_server(i + 1);
                if (peer_fd < 0) {
                    fprintf(stderr, "Failed to connect to library %zu\n", i + 1);
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
    }



    
    char eot = END_OF_TRANSMISSION;
    write(fd, &eot, 1);  // Signal end of transmission
    pthread_mutex_unlock(&global_book_vector.mutex);
}

void handle_return(int fd, requestId reqId, SenderType sender_type, const char* sender_id,
                   const char* book_title) {
    printf("Handling return request: reqId=%d, sender_type=%d, sender_id=%s, book_title=%s\n",
           reqId, sender_type, sender_id, book_title);

    send_argument(fd, operationType_to_char(OP_ANSWER));  // Operation type
    send_argument(fd, reqId_to_char(reqId));              // reqId

    pthread_mutex_lock(&global_book_vector.mutex);

    bool book_found = false;
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            if (book->status == BORROWED) {
                book->status = AVAILABLE;
                send_argument(fd, resultCode_to_char(RESULT_SUCCESS));  // Result code
            } else {
                send_argument(fd, resultCode_to_char(RESULT_FAILURE));  // Result code
            }
            book_found = true;
            break;
        }
    }
    if (!book_found) {
        send_argument(fd, resultCode_to_char(RESULT_FAILURE));  // Result code
    }

    char eot = END_OF_TRANSMISSION;
    write(fd, &eot, 1);  // Signal end of transmission

    pthread_mutex_unlock(&global_book_vector.mutex);
}

void handle_get_users(int fd, requestId reqId) {
    printf("Handling get users request: reqId=%d\n", reqId);

    send_argument(fd, operationType_to_char(OP_USERS_RESULT));  // Operation type
    send_argument(fd, reqId_to_char(reqId));                    // reqId

    pthread_mutex_lock(&global_user_vector.mutex);
    send_argument(fd, size_t_to_char(global_user_vector.size));  // Result code

    for (size_t i = 0; i < global_user_vector.size; ++i) {
        RegisteredUser* user = &global_user_vector.data[i];
        send_argument(fd, user->name);
    }
    char eot = END_OF_TRANSMISSION;
    write(fd, &eot, 1);

    pthread_mutex_unlock(&global_user_vector.mutex);
}

void handle_users_result(int fd, requestId reqId, int user_count, const char** users) {
    (void)fd;
    (void)reqId;
    (void)user_count;
    (void)users;
}

void handle_get_books(int fd, requestId reqId) {
    printf("Handling get books request: reqId=%d\n", reqId);

    pthread_mutex_lock(&global_book_vector.mutex);

    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        send_argument(fd, book->title);
    }
    char eot = END_OF_TRANSMISSION;
    write(fd, &eot, 1);

    pthread_mutex_unlock(&global_book_vector.mutex);
}

void handle_books_result(int fd, requestId reqId, int book_count, const char** books) {
    (void)fd;
    (void)reqId;
    (void)book_count;
    (void)books;
}