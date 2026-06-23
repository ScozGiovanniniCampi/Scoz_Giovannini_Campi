/* Expose POSIX interface strdup in string.h. Suppress warning because POSIX macro names begin with a reserved underscore. */
#define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE      // NOLINT(bugprone-reserved-identifier)

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/vector.h"
#include "network/socket.h"
#include "operations.h"
#include "utils/util.h"

void handle_register(int socket_fd, const char* username) {
    printf("[Library %u] Handling register request: username=%s\n", global_library_id, username);

    ResultCode res_code = RESULT_SUCCESS;

    pthread_mutex_lock(&global_user_vector.mutex);
    bool already_registered = false;
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, username) == 0) {
            already_registered = true;
            break;
        }
    }

    if (already_registered) {
        res_code = ERROR_USER_ALREADY_REGISTERED;
    } else {
        RegisteredUser user;
        memset(&user, 0, sizeof(user));
        user.name = strdup(username);
        if (!user.name) {
            res_code = RESULT_FAILURE;
        } else {
            user.hasBorrowedBook = false;
            if (!add_user_to_vector(&user)) {
                free(user.name);
                res_code = RESULT_FAILURE;
            }
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));
    send_argument(socket_fd, resultCode_to_char(res_code));

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}
