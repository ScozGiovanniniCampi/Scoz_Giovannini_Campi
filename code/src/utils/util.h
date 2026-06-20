#ifndef UTIL_H
#define UTIL_H

#include "network/messages.h"

extern unsigned int global_library_id;
extern unsigned int global_num_total_libraries;

size_t read_argument(int socket_fd, char** buffer);
OperationType read_operator(int socket_fd);

#endif  // UTIL_H
