/* Expose POSIX interface strdup in string.h. Suppress warning because POSIX macro names begin with a reserved underscore. */
#define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE      // NOLINT(bugprone-reserved-identifier)

#include "operations.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "models/book.h"
#include "models/vector.h"
#include "network/messages.h"
#include "network/socket.h"
#include "utils/book_loader.h"
#include "utils/util.h"

void handle_register(int socket_fd, requestId reqId, const char* username) {
    printf("[Library %u] Handling register request: reqId=%d, username=%s\n", global_library_id, reqId, username);

    RegisteredUser user;
    memset(&user, 0, sizeof(user));
    strncpy(user.name, username, MAX_USER_LENGTH - 1);
    user.name[MAX_USER_LENGTH - 1] = '\0';
    user.hasBorrowedBook = false;

    pthread_mutex_lock(&global_user_vector.mutex);
    bool success = add_user_to_vector(&user);
    pthread_mutex_unlock(&global_user_vector.mutex);

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));
    send_argument(socket_fd, reqId_to_char(reqId));
    send_argument(socket_fd, resultCode_to_char(success ? RESULT_SUCCESS : RESULT_FAILURE));

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}

static bool title_matches(SearchType search_type, const char* search_term, const Book* book) {
    if (!search_term) {
        return false;
    }

    switch (search_type) {
        case SEARCH_BY_TITLE:
            return (search_term[0] == '\0' || strstr(book->title, search_term) != NULL) != 0;
        case SEARCH_BY_AUTHOR:
            return (search_term[0] == '\0' || strstr(book->author, search_term) != NULL) != 0;
        case SEARCH_BY_YEAR: {
            char* endptr = NULL;
            long year = strtol(search_term, &endptr, 10);
            return (endptr && *endptr == '\0' && book->publicationYear == (int)year) != 0;
        }
        default:
            return false;
    }
}

static bool search_list_contains(const char** titles, size_t count, const char* title) {
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(titles[i], title) == 0) {
            return true;
        }
    }
    return false;
}

static void free_search_results(char** results, size_t count) {
    if (!results) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        free(results[i]);
    }
    free((void*)results);
}

static bool add_search_title(char*** titles_out, size_t* count_out, size_t* capacity_out, const char* title) {
    if (*count_out > 0 && search_list_contains((const char**)*titles_out, *count_out, title)) {
        return true;
    }

    if (*count_out >= *capacity_out) {
        size_t new_capacity = *capacity_out == 0 ? 4 : *capacity_out * 2;
        char** new_titles = (char**)realloc((void*)*titles_out, new_capacity * sizeof(char*));
        if (!new_titles) {
            perror("realloc");
            return false;
        }
        *titles_out = new_titles;
        *capacity_out = new_capacity;
    }

    char* title_copy = strdup(title);
    if (!title_copy) {
        perror("strdup");
        return false;
    }
    (*titles_out)[(*count_out)++] = title_copy;
    return true;
}

static char** collect_local_search_results(SearchType search_type, const char* search_term, size_t* out_count) {
    char** results = NULL;
    size_t count = 0;
    size_t capacity = 0;

    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (title_matches(search_type, search_term, book)) {
            if (!add_search_title(&results, &count, &capacity, book->title)) {
                free_search_results(results, count);
                results = NULL;
                count = 0;
                break;
            }
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    *out_count = count;
    return results;
}

static void send_search_request(int peer_fd, requestId reqId, SearchType search_type, const char* search_term) {
    send_argument(peer_fd, operationType_to_char(OP_SEARCH));
    send_argument(peer_fd, reqId_to_char(reqId));
    send_argument(peer_fd, senderType_to_char(SENDER_LIBRARY));
    send_argument(peer_fd, searchType_to_char(search_type));
    send_argument(peer_fd, search_term);
    write(peer_fd, &(char){END_OF_TRANSMISSION}, 1);
}

static bool process_peer_search_response(int peer_fd, unsigned int library_id, char*** results, size_t* count, size_t* capacity) {
    char** peer_args = NULL;
    size_t* peer_sizes = NULL;
    int peer_counter = 0;
    OperationType peer_op_code = fetch_arguments(peer_fd, &peer_args, &peer_sizes, &peer_counter);
    bool success = true;

    if (peer_op_code != OP_SEARCH_RESULT || peer_args == NULL || peer_counter < 2) {
        if (peer_op_code != OP_SEARCH_RESULT) {
            fprintf(stderr, "Unexpected operation code from library %d: %d\n", library_id, peer_op_code);
        } else {
            fprintf(stderr, "Invalid search response from library %d: counter=%d\n", library_id, peer_counter);
        }
    } else {
        int book_count = (int)strtol(peer_args[1], NULL, 10);
        if (peer_counter == 2 + book_count) {
            for (int j = 0; j < book_count; ++j) {
                const char* title = peer_args[2 + j];
                if (!add_search_title(results, count, capacity, title)) {
                    success = false;
                    break;
                }
            }
        } else {
            fprintf(stderr, "Malformed search response from library %d: expected %d titles, got %d\n", library_id, book_count, peer_counter - 2);
        }
    }

    if (peer_args) {
        for (int j = 0; j < peer_counter; ++j) {
            free(peer_args[j]);
        }
        free((void*)peer_args);
    }
    free(peer_sizes);
    return success;
}

// TODO: change to use threads over for loop
static char** collect_remote_search_results(requestId reqId, SearchType search_type, const char* search_term, size_t* out_count) {
    char** results = NULL;
    size_t count = 0;
    size_t capacity = 0;
    bool failed = false;

    for (unsigned int i = 0; i < global_num_total_libraries && !failed; ++i) {
        if (i == global_library_id) {
            continue;
        }

        int peer_fd = socket_connect_to_server((int)i);
        if (peer_fd < 0) {
            fprintf(stderr, "Failed to connect to library %d\n", i);
            continue;
        }

        send_search_request(peer_fd, reqId, search_type, search_term);

        if (!process_peer_search_response(peer_fd, i, &results, &count, &capacity)) {
            free_search_results(results, count);
            results = NULL;
            count = 0;
            failed = true;
        }

        close(peer_fd);
    }

    *out_count = count;
    return results;
}

void handle_search(int socket_fd, requestId reqId, SenderType sender_type, SearchType search_type, const char* search_term) {
    printf("[Library %u] Handling search request: reqId=%d, sender_type=%d, search_type=%d, search_term=%s\n", global_library_id, reqId, sender_type, search_type, search_term ? search_term : "");

    size_t local_count = 0;
    char** local_results = collect_local_search_results(search_type, search_term, &local_count);

    size_t remote_count = 0;
    char** remote_results = NULL;
    if (sender_type != SENDER_LIBRARY) {
        remote_results = collect_remote_search_results(reqId, search_type, search_term, &remote_count);
    }

    char** final_results = NULL;
    size_t final_count = 0;
    size_t final_capacity = 0;
    bool final_failed = false;

    for (size_t i = 0; i < local_count; ++i) {
        if (!add_search_title(&final_results, &final_count, &final_capacity, local_results[i])) {
            fprintf(stderr, "Search aggregation failed while adding local results\n");
            final_failed = true;
            break;
        }
    }
    for (size_t i = 0; i < remote_count && !final_failed; ++i) {
        if (!add_search_title(&final_results, &final_count, &final_capacity, remote_results[i])) {
            fprintf(stderr, "Search aggregation failed while adding remote results\n");
            // Clang linter marks this as never read but it's the loop termination flag
            final_failed = true;  // NOLINT: (clang-analyzer-deadcode)
            break;
        }
    }

    send_argument(socket_fd, operationType_to_char(OP_SEARCH_RESULT));
    send_argument(socket_fd, reqId_to_char(reqId));
    send_argument(socket_fd, size_t_to_char(final_count));
    for (size_t i = 0; i < final_count; ++i) {
        send_argument(socket_fd, final_results[i]);
    }
    write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);

    free_search_results(local_results, local_count);
    free_search_results(remote_results, remote_count);
    free_search_results(final_results, final_count);
}

void cleanup(char*** peer_args, size_t** peer_sizes, size_t counter) {
    if (*peer_args) {
        for (size_t i = 0; i < counter; ++i) {
            free((*peer_args)[i]);
        }
        free((void*)*peer_args);
    }
    free(*peer_sizes);
}

static ResultCode get_borrow_response(int peer_fd, requestId reqId, int library_id, const char* book_title) {
    (void)book_title;
    char** peer_args = NULL;
    size_t* peer_sizes = NULL;
    int counter = 0;
    OperationType peer_op_code = fetch_arguments(peer_fd, &peer_args, &peer_sizes, &counter);
    ResultCode res_code = ERROR_BOOK_NOT_FOUND;

    if (peer_op_code != OP_ANSWER || peer_args == NULL || counter < 2) {
        if (peer_op_code != OP_ANSWER) {
            fprintf(stderr, "Unexpected operation code from library %d: %d\n", library_id, peer_op_code);
        } else {
            fprintf(stderr, "Invalid borrow response from library %d: counter=%d\n", library_id, counter);
        }
        cleanup(&peer_args, &peer_sizes, counter);
        return res_code;
    }

    requestId peer_reqId = char_to_reqId(peer_args[0]);
    if (peer_reqId != reqId) {
        fprintf(stderr, "Mismatched reqId from library %d: expected %u, got %u\n", library_id, reqId, peer_reqId);
        cleanup(&peer_args, &peer_sizes, counter);
        return res_code;
    }

    res_code = char_to_resultCode(peer_args[1]);
    return res_code;
}

// TODO: change to use threads over for loop
static ResultCode borrow_from_remote_libraries(requestId reqId, const char* book_title, int* library_id_out) {
    for (unsigned int i = 0; i < global_num_total_libraries; ++i) {
        if (i == global_library_id) {
            continue;  // Skip the current library
        }
        int peer_fd = socket_connect_to_server((int)i);
        if (peer_fd < 0) {
            fprintf(stderr, "Failed to connect to library %d\n", i);
            continue;
        }

        send_argument(peer_fd, operationType_to_char(OP_BORROW));
        send_argument(peer_fd, reqId_to_char(reqId));
        send_argument(peer_fd, senderType_to_char(SENDER_LIBRARY));
        send_argument(peer_fd, unsigned_int_to_char(global_library_id));
        send_argument(peer_fd, book_title);
        write(peer_fd, &(char){END_OF_TRANSMISSION}, 1);  // Signal end of transmission

        ResultCode resCode = get_borrow_response(peer_fd, reqId, (int)i, book_title);
        close(peer_fd);

        if (resCode == RESULT_SUCCESS || resCode == ERROR_BOOK_ALREADY_BORROWED) {
            *library_id_out = (int)i;
            return resCode;
        }
    }

    return ERROR_BOOK_NOT_FOUND;  // Indicate book not found in any library
}

void handle_borrow(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling borrow request: reqId=%d, sender_type=%d, sender_id=%s, book_title=%s\n", global_library_id, reqId, sender_type, sender_id, book_title);

    bool book_found = false;
    ResultCode res_code = RESULT_SUCCESS;

    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            if (book->status == AVAILABLE) {
                book->status = BORROWED;
                res_code = RESULT_SUCCESS;
            } else {
                res_code = ERROR_BOOK_ALREADY_BORROWED;
            }
            book_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));  // Operation type
    send_argument(socket_fd, reqId_to_char(reqId));              // reqId

    if (book_found) {
        send_argument(socket_fd, resultCode_to_char(res_code));  // Result code
    } else if (sender_type == SENDER_USER) {
        int library_id = -1;
        res_code = borrow_from_remote_libraries(reqId, book_title, &library_id);
        send_argument(socket_fd, resultCode_to_char(res_code));  // Result code

    } else {
        send_argument(socket_fd, resultCode_to_char(ERROR_BOOK_NOT_FOUND));  // Result code
    }

    write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);  // Signal end of transmission
}

void handle_return(int socket_fd, requestId reqId, SenderType sender_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling return request: reqId=%d, sender_type=%d, sender_id=%s, book_title=%s\n", global_library_id, reqId, sender_type, sender_id, book_title);

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));  // Operation type
    send_argument(socket_fd, reqId_to_char(reqId));              // reqId

    bool book_found = false;
    ResultCode res_code = RESULT_FAILURE;

    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            if (book->status == BORROWED) {
                book->status = AVAILABLE;
                res_code = RESULT_SUCCESS;
            } else {
                res_code = RESULT_FAILURE;
            }
            book_found = true;
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    if (book_found) {
        send_argument(socket_fd, resultCode_to_char(res_code));  // Result code
    } else {
        send_argument(socket_fd, resultCode_to_char(RESULT_FAILURE));  // Result code
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);  // Signal end of transmission
}

void handle_get_users(int socket_fd, requestId reqId) {
    printf("[Library %u] Handling get users request: reqId=%d\n", global_library_id, reqId);

    send_argument(socket_fd, operationType_to_char(OP_USERS_RESULT));  // Operation type
    send_argument(socket_fd, reqId_to_char(reqId));                    // reqId

    pthread_mutex_lock(&global_user_vector.mutex);
    size_t count = global_user_vector.size;
    char (*temp_users)[MAX_USER_LENGTH] = NULL;
    if (count > 0) {
        temp_users = malloc(count * sizeof(*temp_users));
        if (temp_users) {
            for (size_t i = 0; i < count; ++i) {
                strlcpy(temp_users[i], global_user_vector.data[i].name, sizeof(temp_users[i]));
            }
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);

    if (count > 0 && !temp_users) {
        send_argument(socket_fd, size_t_to_char(0));
    } else {
        send_argument(socket_fd, size_t_to_char(count));
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_users[i]);
        }
        free(temp_users);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}

void handle_get_books(int socket_fd, requestId reqId) {
    printf("[Library %u] Handling get books request: reqId=%d\n", global_library_id, reqId);

    pthread_mutex_lock(&global_book_vector.mutex);
    size_t count = global_book_vector.size;
    const char** temp_titles = NULL;
    if (count > 0) {
        temp_titles = (const char**)malloc(count * sizeof(char*));
        if (temp_titles) {
            for (size_t i = 0; i < count; ++i) {
                temp_titles[i] = global_book_vector.data[i].title;
            }
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    if (temp_titles) {
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_titles[i]);
        }
        free((void*)temp_titles);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}
