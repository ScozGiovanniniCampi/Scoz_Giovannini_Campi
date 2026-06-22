/* Expose POSIX interface strdup in string.h. Suppress warning because POSIX macro names begin with a reserved underscore. */
#define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE      // NOLINT(bugprone-reserved-identifier)

#include "operations.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "models/book.h"
#include "models/vector.h"
#include "network/messages.h"
#include "network/socket.h"
#include "utils/book_loader.h"
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

static bool match_wildcard(const char* pattern, const char* str) {
    if (*pattern == '\0') {
        return *str == '\0';
    }
    if (*pattern == '\\') {
        if (*(pattern + 1) == '\0') {
            return *str == '\\' && *(str + 1) == '\0';
        }
        return (*(pattern + 1) == *str) && match_wildcard(pattern + 2, str + 1);
    }
    if (*pattern == '*') {
        while (*pattern == '*') {
            pattern++;
        }
        if (*pattern == '\0') {
            return true;
        }
        while (*str != '\0') {
            if (match_wildcard(pattern, str)) {
                return true;
            }
            str++;
        }
        return false;
    }
    if (*pattern == '?') {
        if (*str == '\0') {
            return false;
        }
        return match_wildcard(pattern + 1, str + 1);
    }
    return (*pattern == *str) && match_wildcard(pattern + 1, str + 1);
}

static bool has_wildcards(const char* str) { return strchr(str, '*') != NULL || strchr(str, '?') != NULL || strchr(str, '\\') != NULL; }

static bool title_matches(SearchType search_type, const char* search_term, const Book* book) {
    if (!search_term) {
        return false;
    }

    switch (search_type) {
        case SEARCH_BY_TITLE:
            if (has_wildcards(search_term)) {
                return match_wildcard(search_term, book->title);
            }
            return (search_term[0] == '\0' || strcasecmp(book->title, search_term) == 0) != 0;
        case SEARCH_BY_AUTHOR:
            if (has_wildcards(search_term)) {
                return match_wildcard(search_term, book->author);
            }
            return (search_term[0] == '\0' || strcasecmp(book->author, search_term) == 0) != 0;
        case SEARCH_BY_YEAR: {
            if (has_wildcards(search_term)) {
                char year_str[12];
                snprintf(year_str, sizeof(year_str), "%d", book->publicationYear);
                return match_wildcard(search_term, year_str);
            }

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

static void send_search_request(int peer_fd, SearchType search_type, const char* search_term) {
    send_argument(peer_fd, operationType_to_char(OP_SEARCH));
    send_argument(peer_fd, userType_to_char(USER_LIBRARY));
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

    if (peer_op_code != OP_SEARCH_RESULT || peer_args == NULL || peer_counter < 1) {
        if (peer_op_code != OP_SEARCH_RESULT) {
            fprintf(stderr, "Unexpected operation code from library %d: %d\n", library_id, peer_op_code);
        } else {
            fprintf(stderr, "Invalid search response from library %d: counter=%d\n", library_id, peer_counter);
        }
    } else {
        int book_count = (int)strtol(peer_args[0], NULL, 10);
        if (peer_counter == 1 + book_count) {
            for (int j = 0; j < book_count; ++j) {
                const char* title = peer_args[1 + j];
                if (!add_search_title(results, count, capacity, title)) {
                    success = false;
                    break;
                }
            }
        } else {
            fprintf(stderr, "Malformed search response from library %d: expected %d titles, got %d\n", library_id, book_count, peer_counter - 1);
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

typedef struct {
    SearchType search_type;
    const char* search_term;
    unsigned int peer_library_id;
    char** results;
    size_t count;
    bool connection_failed;
    bool response_failed;
} SearchThreadArg;

static void* search_remote_library_thread(void* arg) {
    SearchThreadArg* search_arg = (SearchThreadArg*)arg;
    int peer_fd = socket_connect_to_server((int)search_arg->peer_library_id);
    if (peer_fd < 0) {
        fprintf(stderr, "Failed to connect to library %d\n", search_arg->peer_library_id);
        search_arg->connection_failed = true;
        return NULL;
    }

    send_search_request(peer_fd, search_arg->search_type, search_arg->search_term);

    size_t capacity = 0;
    if (!process_peer_search_response(peer_fd, search_arg->peer_library_id, &search_arg->results, &search_arg->count, &capacity)) {
        search_arg->response_failed = true;
    }

    close(peer_fd);
    return NULL;
}

static unsigned int launch_search_threads(pthread_t* threads, SearchThreadArg* args, SearchType search_type, const char* search_term) {
    unsigned int thread_index = 0;
    for (unsigned int i = 0; i < global_num_total_libraries; ++i) {
        if (i == global_library_id) {
            continue;
        }

        args[thread_index].search_type = search_type;
        args[thread_index].search_term = search_term;
        args[thread_index].peer_library_id = i;
        args[thread_index].results = NULL;
        args[thread_index].count = 0;
        args[thread_index].connection_failed = false;
        args[thread_index].response_failed = false;

        if (pthread_create(&threads[thread_index], NULL, search_remote_library_thread, &args[thread_index]) != 0) {
            perror("pthread_create");
            args[thread_index].response_failed = true;
        } else {
            thread_index++;
        }
    }
    return thread_index;
}

static bool aggregate_search_results(SearchThreadArg* args, unsigned int actual_threads, char*** final_results, size_t* final_count, size_t* final_capacity) {
    for (unsigned int i = 0; i < actual_threads; ++i) {
        if (args[i].connection_failed || args[i].response_failed) {
            continue;
        }
        for (size_t j = 0; j < args[i].count; ++j) {
            if (!add_search_title(final_results, final_count, final_capacity, args[i].results[j])) {
                return false;
            }
        }
    }
    return true;
}

static char** collect_remote_search_results(SearchType search_type, const char* search_term, size_t* out_count) {
    unsigned int num_threads = 0;
    if (global_num_total_libraries > 1) {
        num_threads = global_num_total_libraries - 1;
    }

    if (num_threads == 0) {
        *out_count = 0;
        return NULL;
    }

    pthread_t* threads = calloc(num_threads, sizeof(pthread_t));
    SearchThreadArg* args = calloc(num_threads, sizeof(SearchThreadArg));
    if (!threads || !args) {
        perror("calloc");
        free(threads);
        free(args);
        *out_count = 0;
        return NULL;
    }

    unsigned int actual_threads = launch_search_threads(threads, args, search_type, search_term);

    bool failed = false;
    for (unsigned int i = 0; i < actual_threads; ++i) {
        pthread_join(threads[i], NULL);
        if (args[i].response_failed) {
            failed = true;
        }
    }

    char** final_results = NULL;
    size_t final_count = 0;
    size_t final_capacity = 0;

    if (!failed) {
        if (!aggregate_search_results(args, actual_threads, &final_results, &final_count, &final_capacity)) {
            failed = true;
        }
    }

    // Cleanup all thread results
    for (unsigned int i = 0; i < actual_threads; ++i) {
        free_search_results(args[i].results, args[i].count);
    }

    free(threads);
    free(args);

    if (failed) {
        free_search_results(final_results, final_count);
        *out_count = 0;
        return NULL;
    }

    *out_count = final_count;
    return final_results;
}

void handle_search(int socket_fd, UserType user_type, SearchType search_type, const char* search_term) {
    printf("[Library %u] Handling search request: user_type=%d, search_type=%d, search_term=%s\n", global_library_id, user_type, search_type, search_term ? search_term : "");

    size_t local_count = 0;
    char** local_results = collect_local_search_results(search_type, search_term, &local_count);

    size_t remote_count = 0;
    char** remote_results = NULL;
    if (user_type != USER_LIBRARY) {
        remote_results = collect_remote_search_results(search_type, search_term, &remote_count);
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

static ResultCode get_borrow_response(int peer_fd, int library_id, const char* book_title) {
    (void)book_title;
    char** peer_args = NULL;
    size_t* peer_sizes = NULL;
    int counter = 0;
    OperationType peer_op_code = fetch_arguments(peer_fd, &peer_args, &peer_sizes, &counter);
    ResultCode res_code = ERROR_BOOK_NOT_FOUND;

    if (peer_op_code != OP_ANSWER || peer_args == NULL || counter < 1) {
        if (peer_op_code != OP_ANSWER) {
            fprintf(stderr, "Unexpected operation code from library %d: %d\n", library_id, peer_op_code);
        } else {
            fprintf(stderr, "Invalid borrow response from library %d: counter=%d\n", library_id, counter);
        }
        cleanup(&peer_args, &peer_sizes, counter);
        return res_code;
    }

    res_code = char_to_resultCode(peer_args[0]);
    return res_code;
}

typedef struct {
    const char* book_title;
    unsigned int peer_library_id;
    ResultCode result_code;
} BorrowThreadArg;

static void* borrow_remote_library_thread(void* arg) {
    BorrowThreadArg* borrow_arg = (BorrowThreadArg*)arg;
    int peer_fd = socket_connect_to_server((int)borrow_arg->peer_library_id);
    if (peer_fd < 0) {
        fprintf(stderr, "Failed to connect to library %d\n", borrow_arg->peer_library_id);
        borrow_arg->result_code = ERROR_BOOK_NOT_FOUND;
        return NULL;
    }

    send_argument(peer_fd, operationType_to_char(OP_BORROW));
    send_argument(peer_fd, userType_to_char(USER_LIBRARY));
    send_argument(peer_fd, unsigned_int_to_char(global_library_id));
    send_argument(peer_fd, borrow_arg->book_title);
    write(peer_fd, &(char){END_OF_TRANSMISSION}, 1);  // Signal end of transmission

    borrow_arg->result_code = get_borrow_response(peer_fd, (int)borrow_arg->peer_library_id, borrow_arg->book_title);
    close(peer_fd);
    return NULL;
}

static ResultCode borrow_from_remote_libraries(const char* book_title, int* library_id_out) {
    unsigned int num_threads = 0;
    if (global_num_total_libraries > 1) {
        num_threads = global_num_total_libraries - 1;
    }

    if (num_threads == 0) {
        return ERROR_BOOK_NOT_FOUND;
    }

    pthread_t* threads = calloc(num_threads, sizeof(pthread_t));
    BorrowThreadArg* args = calloc(num_threads, sizeof(BorrowThreadArg));
    if (!threads || !args) {
        perror("calloc");
        free(threads);
        free(args);
        return ERROR_BOOK_NOT_FOUND;
    }

    unsigned int thread_index = 0;
    for (unsigned int i = 0; i < global_num_total_libraries; ++i) {
        if (i == global_library_id) {
            continue;  // Skip the current library
        }

        args[thread_index].book_title = book_title;
        args[thread_index].peer_library_id = i;
        args[thread_index].result_code = ERROR_BOOK_NOT_FOUND;

        if (pthread_create(&threads[thread_index], NULL, borrow_remote_library_thread, &args[thread_index]) != 0) {
            perror("pthread_create");
            args[thread_index].result_code = ERROR_BOOK_NOT_FOUND;
        } else {
            thread_index++;
        }
    }

    unsigned int actual_threads = thread_index;
    ResultCode final_result = ERROR_BOOK_NOT_FOUND;
    int success_library_id = -1;

    for (unsigned int i = 0; i < actual_threads; ++i) {
        pthread_join(threads[i], NULL);
        if (args[i].result_code == RESULT_SUCCESS || args[i].result_code == ERROR_BOOK_ALREADY_BORROWED) {
            final_result = args[i].result_code;
            success_library_id = (int)args[i].peer_library_id;
        }
    }

    free(threads);
    free(args);

    if (success_library_id != -1) {
        *library_id_out = success_library_id;
    }
    return final_result;
}

static ResultCode check_user_can_borrow(const char* username) {
    ResultCode code = ERROR_USER_NOT_REGISTERED;
    pthread_mutex_lock(&global_user_vector.mutex);
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, username) == 0) {
            if (global_user_vector.data[i].hasBorrowedBook) {
                code = ERROR_USER_ALREADY_BORROWED_BOOK;
            } else {
                code = RESULT_SUCCESS;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);
    return code;
}

static void set_user_borrow_status(const char* username, bool status) {
    pthread_mutex_lock(&global_user_vector.mutex);
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, username) == 0) {
            global_user_vector.data[i].hasBorrowedBook = status;
            break;
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);
}

static ResultCode return_to_specific_library(const char* book_title, int target_library_id) {
    int peer_fd = socket_connect_to_server(target_library_id);
    if (peer_fd < 0) {
        return ERROR_BOOK_NOT_FOUND;
    }

    send_argument(peer_fd, operationType_to_char(OP_RETURN));
    send_argument(peer_fd, userType_to_char(USER_LIBRARY));
    send_argument(peer_fd, size_t_to_char((size_t)global_library_id));
    send_argument(peer_fd, book_title);
    write(peer_fd, &(char){END_OF_TRANSMISSION}, 1);

    ResultCode resCode = get_borrow_response(peer_fd, target_library_id, book_title);
    close(peer_fd);
    return resCode;
}

void handle_borrow(int socket_fd, UserType user_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling borrow request: user_type=%d, sender_id=%s, book_title=%s\n", global_library_id, user_type, sender_id, book_title);

    ResultCode res_code = RESULT_SUCCESS;
    bool book_found_and_available = false;

    if (user_type == USER_USER) {
        res_code = check_user_can_borrow(sender_id);
        if (res_code != RESULT_SUCCESS) {
            send_argument(socket_fd, operationType_to_char(OP_ANSWER));
            send_argument(socket_fd, resultCode_to_char(res_code));
            write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
            return;
        }
    }

    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            if (book->status == AVAILABLE) {
                book->status = BORROWED;
                res_code = RESULT_SUCCESS;
                book_found_and_available = true;
            } else {
                res_code = ERROR_BOOK_ALREADY_BORROWED;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    int owner_lib_id = (int)global_library_id;
    if (!book_found_and_available && res_code != ERROR_BOOK_ALREADY_BORROWED && user_type == USER_USER) {
        int library_id = -1;
        res_code = borrow_from_remote_libraries(book_title, &library_id);
        if (res_code == RESULT_SUCCESS) {
            book_found_and_available = true;
            owner_lib_id = library_id;
        }
    } else if (!book_found_and_available && res_code != ERROR_BOOK_ALREADY_BORROWED) {
        res_code = ERROR_BOOK_NOT_FOUND;
    }

    if (book_found_and_available && user_type == USER_USER) {
        BorrowedBook record;
        memset(&record, 0, sizeof(record));
        record.book.title = strdup(book_title);
        record.borrowerId = strdup(sender_id);
        record.borrowerType = user_type;
        record.ownerLibraryId = owner_lib_id;

        pthread_mutex_lock(&global_borrowed_book_vector.mutex);
        add_book_to_vector(&global_borrowed_book_vector, &record);
        pthread_mutex_unlock(&global_borrowed_book_vector.mutex);

        set_user_borrow_status(sender_id, true);
    }

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));
    send_argument(socket_fd, resultCode_to_char(res_code));
    write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
}

static ResultCode validate_returning_user(const char* sender_id) {
    ResultCode user_check = ERROR_USER_NOT_REGISTERED;
    bool user_has_borrowed = false;
    pthread_mutex_lock(&global_user_vector.mutex);
    for (size_t i = 0; i < global_user_vector.size; ++i) {
        if (strcmp(global_user_vector.data[i].name, sender_id) == 0) {
            user_has_borrowed = global_user_vector.data[i].hasBorrowedBook;
            user_check = RESULT_SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&global_user_vector.mutex);

    if (user_check != RESULT_SUCCESS) {
        return user_check;
    }
    if (!user_has_borrowed) {
        return ERROR_USER_HAS_NO_BORROWED_BOOK;
    }
    return RESULT_SUCCESS;
}

static bool check_borrowed_specific_book(const char* sender_id, UserType user_type, const char* book_title, int* target_library_id_out) {
    bool borrowed_this_book = false;
    pthread_mutex_lock(&global_borrowed_book_vector.mutex);
    for (size_t i = 0; i < global_borrowed_book_vector.size; ++i) {
        if (strcmp(global_borrowed_book_vector.data[i].borrowerId, sender_id) == 0 && global_borrowed_book_vector.data[i].borrowerType == user_type &&
            strcmp(global_borrowed_book_vector.data[i].book.title, book_title) == 0) {
            borrowed_this_book = true;
            if (target_library_id_out) {
                *target_library_id_out = global_borrowed_book_vector.data[i].ownerLibraryId;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_borrowed_book_vector.mutex);
    return borrowed_this_book;
}

static ResultCode try_return_local_book(const char* book_title, bool* belongs_to_us_out) {
    ResultCode res_code = ERROR_BOOK_NOT_BORROWED;
    bool belongs_to_us = false;
    pthread_mutex_lock(&global_book_vector.mutex);
    for (size_t i = 0; i < global_book_vector.size; ++i) {
        Book* book = &global_book_vector.data[i];
        if (strcmp(book->title, book_title) == 0) {
            belongs_to_us = true;
            if (book->status == BORROWED) {
                book->status = AVAILABLE;
                res_code = RESULT_SUCCESS;
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);
    if (belongs_to_us_out) {
        *belongs_to_us_out = belongs_to_us;
    }
    return belongs_to_us ? res_code : ERROR_BOOK_NOT_FOUND;
}

static void cleanup_borrow_record(const char* sender_id, UserType user_type, const char* book_title) {
    pthread_mutex_lock(&global_borrowed_book_vector.mutex);
    for (size_t i = 0; i < global_borrowed_book_vector.size; ++i) {
        if (strcmp(global_borrowed_book_vector.data[i].borrowerId, sender_id) == 0 && global_borrowed_book_vector.data[i].borrowerType == user_type &&
            strcmp(global_borrowed_book_vector.data[i].book.title, book_title) == 0) {
            BorrowedBook* removed = remove_book_from_vector_borrowed(&global_borrowed_book_vector, i);
            free(removed->book.title);
            free(removed->borrowerId);
            free(removed);
            break;
        }
    }
    pthread_mutex_unlock(&global_borrowed_book_vector.mutex);

    set_user_borrow_status(sender_id, false);
}

void handle_return(int socket_fd, UserType user_type, const char* sender_id, const char* book_title) {
    printf("[Library %u] Handling return request: user_type=%d, sender_id=%s, book_title=%s\n", global_library_id, user_type, sender_id, book_title);

    ResultCode res_code = RESULT_FAILURE;
    bool belongs_to_us = false;
    int target_library_id = -1;

    if (user_type == USER_USER) {
        // 1. Check if user is registered and has borrowed a book
        res_code = validate_returning_user(sender_id);
        if (res_code != RESULT_SUCCESS) {
            send_argument(socket_fd, operationType_to_char(OP_ANSWER));
            send_argument(socket_fd, resultCode_to_char(res_code));
            write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
            return;
        }

        // 2. Check if the user borrowed this specific book
        if (!check_borrowed_specific_book(sender_id, user_type, book_title, &target_library_id)) {
            send_argument(socket_fd, operationType_to_char(OP_ANSWER));
            send_argument(socket_fd, resultCode_to_char(ERROR_BOOK_NOT_BORROWED_BY_USER));
            write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
            return;
        }
    }

    // 3. Try return locally
    res_code = try_return_local_book(book_title, &belongs_to_us);

    // 4. Try return remotely if needed
    if (!belongs_to_us && user_type == USER_USER) {
        res_code = (target_library_id != -1) ? return_to_specific_library(book_title, target_library_id) : ERROR_BOOK_NOT_FOUND;
    }

    // 5. Clean up borrow record on success
    if (res_code == RESULT_SUCCESS && user_type == USER_USER) {
        cleanup_borrow_record(sender_id, user_type, book_title);
    }

    send_argument(socket_fd, operationType_to_char(OP_ANSWER));
    send_argument(socket_fd, resultCode_to_char(res_code));
    write(socket_fd, &(char){END_OF_TRANSMISSION}, 1);
}

void handle_get_users(int socket_fd) {
    printf("[Library %u] Handling get users request\n", global_library_id);

    send_argument(socket_fd, operationType_to_char(OP_USERS_RESULT));

    pthread_mutex_lock(&global_user_vector.mutex);
    pthread_mutex_lock(&global_borrowed_book_vector.mutex);

    size_t count = global_user_vector.size;
    char** temp_users = NULL;
    char** temp_books = NULL;

    if (count > 0) {
        temp_users = (char**)malloc(count * sizeof(char*));
        temp_books = (char**)malloc(count * sizeof(char*));
        for (size_t i = 0; i < count; ++i) {
            temp_users[i] = strdup(global_user_vector.data[i].name);
            temp_books[i] = strdup("None");

            if (global_user_vector.data[i].hasBorrowedBook) {
                for (size_t j = 0; j < global_borrowed_book_vector.size; ++j) {
                    if (strcmp(global_borrowed_book_vector.data[j].borrowerId, temp_users[i]) == 0) {
                        free(temp_books[i]);
                        temp_books[i] = strdup(global_borrowed_book_vector.data[j].book.title);
                        break;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&global_borrowed_book_vector.mutex);
    pthread_mutex_unlock(&global_user_vector.mutex);

    send_argument(socket_fd, size_t_to_char(count));

    if (count > 0) {
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_users[i]);
            send_argument(socket_fd, temp_books[i]);
            free(temp_users[i]);
            free(temp_books[i]);
        }
        free((void*)temp_users);
        free((void*)temp_books);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}

void handle_get_books(int socket_fd) {
    printf("[Library %u] Handling get books request\n", global_library_id);

    send_argument(socket_fd, operationType_to_char(OP_BOOKS_RESULT));

    pthread_mutex_lock(&global_book_vector.mutex);
    size_t count = global_book_vector.size;

    char** temp_titles = NULL;
    char** temp_statuses = NULL;

    if (count > 0) {
        temp_titles = (char**)malloc(count * sizeof(char*));
        temp_statuses = (char**)malloc(count * sizeof(char*));
        for (size_t i = 0; i < count; ++i) {
            temp_titles[i] = strdup(global_book_vector.data[i].title);
            if (global_book_vector.data[i].status == AVAILABLE) {
                temp_statuses[i] = strdup("AVAILABLE");
            } else {
                temp_statuses[i] = strdup("BORROWED");
            }
        }
    }
    pthread_mutex_unlock(&global_book_vector.mutex);

    send_argument(socket_fd, size_t_to_char(count));

    if (count > 0) {
        for (size_t i = 0; i < count; ++i) {
            send_argument(socket_fd, temp_titles[i]);
            send_argument(socket_fd, temp_statuses[i]);
            free(temp_titles[i]);
            free(temp_statuses[i]);
        }
        free((void*)temp_titles);
        free((void*)temp_statuses);
    }

    char eot = END_OF_TRANSMISSION;
    write(socket_fd, &eot, 1);
}
