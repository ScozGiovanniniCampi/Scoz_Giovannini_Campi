/* Expose POSIX interface getline in stdio.h. Suppress warning because POSIX macro names begin with a reserved underscore. */
#define _POSIX_C_SOURCE 200809L  // NOLINT(bugprone-reserved-identifier)

#include "book_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char* parse_quoted_field(char* src, char* dst, size_t dst_size) {
    size_t index = 0;
    src++; /* consume opening quote */
    while (*src) {
        if (*src == '"' && src[1] == '"') {
            if (index + 1 < dst_size) {
                dst[index++] = '"';
            }
            src += 2;
            continue;
        }
        if (*src == '"') {
            src++; /* closing quote */
            break;
        }
        if (index + 1 < dst_size) {
            dst[index++] = *src;
        }
        src++;
    }
    if (*src == ',') {
        src++;
    }
    dst[index] = '\0';
    return src;
}

static char* parse_unquoted_field(char* src, char* dst, size_t dst_size) {
    size_t index = 0;
    while (*src && *src != ',') {
        if (index + 1 < dst_size) {
            dst[index++] = *src;
        }
        src++;
    }
    if (*src == ',') {
        src++;
    }
    dst[index] = '\0';
    return src;
}

static void parse_field(char** field_ptr, char* dst, size_t dst_size) {
    char* src = *field_ptr;
    if (*src == '"') {
        src = parse_quoted_field(src, dst, dst_size);
    } else {
        src = parse_unquoted_field(src, dst, dst_size);
    }
    while (*src == ' ' || *src == '\t') {
        src++;
    }
    *field_ptr = src;
}

static int parse_csv_line(char* line, char* title, size_t title_len, char* author, size_t author_len, int* year_out) {
    char* ptr = line;
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }
    parse_field(&ptr, title, title_len);
    parse_field(&ptr, author, author_len);
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }
    if (*ptr == '\0') {
        return 0;
    }
    char* end;
    long year = strtol(ptr, &end, 10);
    if (end == ptr) {
        return 0;
    }
    *year_out = (int)year;
    return 1;
}

BookVector loadBooksFromFile(const char* filename) {
    BookVector books = {0};
    pthread_mutex_init(&books.mutex, NULL);

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Could not open file");
        return books;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t nread;

    /* skip header line (if present) */
    if (getline(&line, &len, file) == -1) {
        free(line);
        fclose(file);
        return books;
    }

    while ((nread = getline(&line, &len, file)) != -1) {
        if (nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }
        char* ptr = line;
        while (*ptr == ' ' || *ptr == '\t') {
            ptr++;
        }
        if (*ptr == '\0') {
            continue;
        }

        Book book;
        if (!parse_csv_line(line, book.title, MAX_TITLE_LENGTH, book.author, MAX_AUTHOR_LENGTH, &book.publicationYear)) {
            continue;
        }
        book.status = AVAILABLE;
        add_book_to_vector(&books, &book);
    }

    free(line);
    fclose(file);
    return books;
}