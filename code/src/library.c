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
    BookVector *books = loadBooksFromFile(argv[2]);

    (void)libraryId;

    if (books) {
        for (size_t i = 0; i < books->size; ++i) {
            Book *b = &books->data[i];
            printf("Book: %s by %s (%d)\n", b->title, b->author, b->publicationYear);
        }
        free_book_vector(books);
        free(books);
    }

    return 0;
}