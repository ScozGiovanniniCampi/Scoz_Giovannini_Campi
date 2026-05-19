#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book.h"
#include "util.h"
#include "book_loader.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ID> <books_file.csv>\n", argv[0]);
        return 1;
    }

    unsigned int libraryId = atoi(argv[1]);
    Book *books = loadBooksFromFile(argv[2]);
    BorrowedBook *borrowedBooks = NULL;

    for (Book *b = books; b != NULL; b = b->hh.next) {
        printf("Book: %s by %s (%d)\n", b->title, b->author, b->publicationYear);
    }

    return 0;
}