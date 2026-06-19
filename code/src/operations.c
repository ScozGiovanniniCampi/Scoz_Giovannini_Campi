#include "operations.h"
#include "util.h"
#include "messages.h"

void handle_answer(int fd, requestId reqId, ResultCode result_code) {

}

void handle_register(int fd, requestId reqId, const char* username) {
    printf("Handling register request: reqId=%d, username=%s\n", reqId, username);

    RegisteredUser user;

    user.username = username;

    wait(global_user_vector.mutex);

    if (!add_user_to_vector(&user)) {
        send_argument(fd, &(char)OP_ANSWER);
        send_argument(fd, reqId_to_char(reqId));
        send_argument(fd, (char)RESULT_FAILURE);
    } else {
        send_argument(fd, &(char)OP_ANSWER);
        send_argument(fd, reqId_to_char(reqId));
        send_argument(fd, (char)RESULT_SUCCESS);
    }

    signal(global_user_vector.mutex);
}

void handle_search(int fd, requestId reqId, SearchType search_type, const char* search_term) {

}

void handle_search_result(int fd, requestId reqId, int book_count, const char** books) {
    
}

void handle_borrow(int fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    
}

void handle_return(int fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {

}

void handle_get_users(int fd, requestId reqId) {

}

void handle_users_result(int fd, requestId reqId, int user_count, const char** users) {

}

void handle_get_books(int fd, requestId reqId) {

}

void handle_books_result(int fd, requestId reqId, int book_count, const char** books) {

}