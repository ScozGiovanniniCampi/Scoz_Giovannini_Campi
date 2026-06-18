#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "operations.h"

void handle_answer(requestId reqId, RESULT_CODE result_code);
void handle_register(requestId reqId, const char* username);
void handle_search(requestId reqId, SearchType search_type, const char* search_term);
void handle_search_result(requestId reqId, int book_count, const char** books);
void handle_borrow(requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title);
void handle_return(requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title);
void handle_get_users(requestId reqId);
void handle_users_result(requestId reqId, int user_count, const char** users);
void handle_get_books(requestId reqId);
void handle_books_result(requestId reqId, int book_count, const char** books);

#endif // OPERATIONS_H