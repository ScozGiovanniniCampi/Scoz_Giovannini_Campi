#include "book.h"
int hash_book_title(const char *title);
#include <string.h> /* for strnlen */

int hash_book_struct(const struct Book *book) {
    return hash_book_title(book->title);
}

int hash_book_title(const char *title) {
    int hash = 0;
    size_t len = strnlen(title, MAX_TITLE_LENGTH);
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)title[i];
        hash += (int)c;
        if (i % 3 == 0) {
            hash *= 3;
        }
        if (i % 5 == 0) {
            hash *= 5;
        }
    }
    return hash;
}
