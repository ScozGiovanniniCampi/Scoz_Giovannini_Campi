#include "book.h"

#include <string.h> /* for strnlen */

unsigned int hash_book_struct(const Book *book) {
    return hash_book_title(book->title);
}

unsigned int hash_book_title(const char *title) {
    unsigned int hash = 0U;
    size_t len = strnlen(title, MAX_TITLE_LENGTH);
    for (size_t i = 0; i < len; ++i) {
        unsigned int c = (unsigned char)title[i];
        hash += c;
        if (i % 3 == 0) {
            hash *= 3U;
        }
        if (i % 5 == 0) {
            hash *= 5U;
        }
    }
    return hash;
}
