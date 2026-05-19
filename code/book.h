#ifndef BOOK_H
#define BOOK_H
#define MAX_TITLE_LENGTH 100
#define MAX_AUTHOR_LENGTH 50

#include "include/uthash.h"

struct Book {
    char title[MAX_TITLE_LENGTH];
    char author[MAX_AUTHOR_LENGTH];
    int publicationYear;
    UT_hash_handle hh; /* makes this structure hashable */
};

int hash_book_struct(const struct Book *book);
int hash_book_title(const char *title);

/** Hash function for a Book object based on its title.
 *
 * This function computes a hash value for a given Book object
 * by iterating throughits title and applying a combination of
 * addition and multiplication with prime numbers to avoid collisions.
 *
 * @param book A pointer to the Book object to be hashed.
 * @return An integer representing the hash value of the book's title.
 */
#define hash_book(x) _Generic((x), \
    struct Book*: hash_book_struct, \
    const char*: hash_book_title \
)(x)

#endif // BOOK_H