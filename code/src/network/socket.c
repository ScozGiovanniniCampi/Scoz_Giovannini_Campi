#include "socket.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/util.h"

int socket_init_server(LibrarySocket* sock, int library_id) {
    sock->library_id = library_id;
    sock->fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock->fd < 0) {
        perror("socket");
        return -1;
    }

    sock->addr.sun_family = AF_UNIX;
    if (snprintf(sock->addr.sun_path, sizeof(sock->addr.sun_path), SOCKET_PATH_TEMPLATE, library_id) < 0) {
        fprintf(stderr, "Socket path is too long\n");
        close(sock->fd);
        return -2;
    }

    memset(sock->addr.sun_path + strlen(sock->addr.sun_path), 0, sizeof(sock->addr.sun_path) - strlen(sock->addr.sun_path));

    if (bind(sock->fd, (struct sockaddr*)&sock->addr, sizeof(sock->addr)) < 0) {
        perror("bind");
        close(sock->fd);
        return -3;
    }

    if (listen(sock->fd, 128) < 0) {
        perror("listen");
        close(sock->fd);
        return -4;
    }

    return 0;
}

int socket_connect_to_server(int peer_id) {
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if (snprintf(addr.sun_path, sizeof(addr.sun_path), SOCKET_PATH_TEMPLATE, peer_id) < 0) {
        fprintf(stderr, "Socket path is too long\n");
        close(socket_fd);
        return -2;
    }

    while (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno == ECONNREFUSED) {
            // Server not ready yet, wait and retry
            sleep(1);  // 1 second
            continue;
        }

        perror("connect");
        close(socket_fd);
        return -3;
    }

    return socket_fd;
}

void socket_close(LibrarySocket* sock) {
    close(sock->fd);
    unlink(sock->addr.sun_path);
}

void send_argument(int socket_fd, const char* arg) {
    size_t arg_len = strlen(arg);
    size_t total_len = arg_len + 1;  // +1 for END_OF_ARGUMENT

    char* message = (char*)malloc(total_len);
    if (!message) {
        perror("malloc");
        return;
    }

    /* Deliberately not null-terminating since our protocol uses ASCII ETX framing instead of a null terminator. */
    memcpy(message, arg, arg_len);  // NOLINT(bugprone-not-null-terminated-result)
    message[arg_len] = END_OF_ARGUMENT;

    ssize_t written = write(socket_fd, message, total_len);
    if (written < 0 || (size_t)written != total_len) {
        perror("send_argument");
        free(message);
        return;
    }

    free(message);
}