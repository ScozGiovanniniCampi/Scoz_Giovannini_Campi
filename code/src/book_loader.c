#include "book_loader.h"
#include <stdio.h>
#include <stdlib.h>

static void parse_field(char **p, char *dst, size_t dst_size) {
    char *s = *p;
    size_t i = 0;
    if (*s == '"') {
        s++; /* consume opening quote */
        while (*s) {
            if (*s == '"') {
                if (s[1] == '"') { /* escaped quote "" -> " */
                    if (i + 1 < dst_size) dst[i++] = '"';
                    s += 2;
                    continue;
                }
                s++; /* closing quote */
                break;
            }
            if (i + 1 < dst_size) dst[i++] = *s;
            s++;
        }
        if (*s == ',') s++;
    } else {
        while (*s && *s != ',') {
            if (i + 1 < dst_size) dst[i++] = *s;
            s++;
        }
        if (*s == ',') s++;
    }
    dst[i] = '\0';
    while (*s == ' ' || *s == '\t') s++;
    *p = s;
}

static int parse_csv_line(char *line, char *title, size_t title_len, char *author, size_t author_len, int *year_out) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    parse_field(&p, title, title_len);
    parse_field(&p, author, author_len);
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return 0;
    char *end;
    long y = strtol(p, &end, 10);
    if (end == p) return 0;
    *year_out = (int)y;
    return 1;
}

BookVector *loadBooksFromFile(const char *filename) {
    BookVector *books = calloc(1, sizeof(*books));
    if (!books) {
        perror("calloc");
        return NULL;
    }
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Could not open file");
        free(books);
        return NULL;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    /* skip header line (if present) */
    if ((nread = getline(&line, &len, file)) == -1) {
        free(line);
        fclose(file);
        free(books);
        return NULL;
    }

    while ((nread = getline(&line, &len, file)) != -1) {
        if (nread > 0 && line[nread - 1] == '\n') line[nread - 1] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') continue;

        Book b;
        if (!parse_csv_line(line, b.title, MAX_TITLE_LENGTH, b.author, MAX_AUTHOR_LENGTH, &b.publicationYear)) {
            continue;
        }
        b.status = AVAILABLE;
        add_book_to_vector(books, &b);
    }

    free(line);
    fclose(file);
    return books;
}