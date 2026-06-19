#include "operations.h"
#include "util.h"
#include "messages.h"
#include "socket.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

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

    if (global_user_vector) {
        pthread_mutex_lock(&global_user_vector->mutex);
    }

    bool success = add_user_to_vector(&user);

    char op_str[12];
    snprintf(op_str, sizeof(op_str), "%d", OP_ANSWER);

    char res_fail_str[12];
    snprintf(res_fail_str, sizeof(res_fail_str), "%d", RESULT_FAILURE);

    char res_succ_str[12];
    snprintf(res_succ_str, sizeof(res_succ_str), "%d", RESULT_SUCCESS);

    if (!success) {
        send_argument(fd, op_str);
        send_argument(fd, reqId_to_char(reqId));
        send_argument(fd, res_fail_str);
    } else {
        send_argument(fd, op_str);
        send_argument(fd, reqId_to_char(reqId));
        send_argument(fd, res_succ_str);
    }

    if (global_user_vector) {
        pthread_mutex_unlock(&global_user_vector->mutex);
    }
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
    (void)fd;
    (void)reqId;
    (void)sender_type;
    (void)sender_id;
    (void)book_title;
}

void handle_return(int fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    (void)fd;
    (void)reqId;
    (void)sender_type;
    (void)sender_id;
    (void)book_title;
}

void handle_get_users(int fd, requestId reqId) {
    (void)fd;
    (void)reqId;
}

void handle_users_result(int fd, requestId reqId, int user_count, const char** users) {
    (void)fd;
    (void)reqId;
    (void)user_count;
    (void)users;
}

void handle_get_books(int fd, requestId reqId) {
    (void)fd;
    (void)reqId;
}

void handle_books_result(int fd, requestId reqId, int book_count, const char** books) {
    (void)fd;
    (void)reqId;
    (void)book_count;
    (void)books;
}