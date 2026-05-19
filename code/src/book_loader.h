#ifndef BOOK_LOADER_H
#define BOOK_LOADER_H

#include "book.h"
#include "util.h"

static void parse_field(char **p, char *dst, size_t dst_size);

static int parse_csv_line(char *line, char *title, size_t title_len, char *author, size_t author_len, int *year_out);

Book *loadBooksFromFile(const char *filename);

#endif /* BOOK_LOADER_H */