#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "network/messages.h"

void handle_register(int socket_fd, requestId reqId, const char* username);
void handle_search(int socket_fd, requestId reqId, SenderType sender_type, SearchType search_type, const char* search_term);
void handle_borrow(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title);
void handle_return(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title);
void handle_get_users(int socket_fd, requestId reqId);
void handle_get_books(int socket_fd, requestId reqId);

#endif  // OPERATIONS_H