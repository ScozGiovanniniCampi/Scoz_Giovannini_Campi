/* Expose POSIX interface strdup in string.h. Suppress warning because POSIX macro names begin with a reserved underscore. */
#define _DEFAULT_SOURCE  // NOLINT(bugprone-reserved-identifier)
#define _GNU_SOURCE      // NOLINT(bugprone-reserved-identifier)

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "models/book.h"
#include "models/vector.h"
#include "network/socket.h"
#include "operations.h"
#include "utils/util.h"

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
