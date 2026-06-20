#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "network/messages.h"

void handle_answer(int socket_fd, requestId reqId, ResultCode result_code);
void handle_register(int socket_fd, requestId reqId, const char* username);
void handle_search(int socket_fd, requestId reqId, SearchType search_type, const char* search_term);
void handle_search_result(int socket_fd, requestId reqId, int book_count, const char** books);
void handle_borrow(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title);
void handle_return(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title);
void handle_get_users(int socket_fd, requestId reqId);
void handle_users_result(int socket_fd, requestId reqId, int user_count, const char** users);
void handle_get_books(int socket_fd, requestId reqId);
void handle_books_result(int socket_fd, requestId reqId, int book_count, const char** books);

#endif  // OPERATIONS_H