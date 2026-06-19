#ifndef LIB_SOCKET_H
#define LIB_SOCKET_H

#include <sys/socket.h>
#include <sys/un.h>

#include "messages.h"

#define SOCKET_PATH_TEMPLATE "/tmp/lib_%d.sock"

// Represents a socket handle
typedef struct {
    int fd;
    int library_id;
    struct sockaddr_un addr;
} LibrarySocket;

// Function signatures to implement
int socket_init_server(LibrarySocket *sock, int library_id);
int socket_connect_to_server(int peer_id);
void socket_close(LibrarySocket *sock);

void send_argument(int fd, const char *arg);

#endif  // LIB_SOCKET_H