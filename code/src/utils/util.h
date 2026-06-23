#ifndef UTIL_H
#define UTIL_H

#include "network/messages.h"

extern unsigned int global_library_id;
extern unsigned int global_num_total_libraries;

size_t read_argument(int socket_fd, char** buffer);
int read_operator(int socket_fd);
int fetch_arguments(int cfd, char*** args_out, size_t** sizes_out, int* counter_out);

#endif  // UTIL_H
