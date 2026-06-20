#ifndef BOOK_LOADER_H
#define BOOK_LOADER_H

#include "models/book.h"
#include "models/vector.h"
#include "utils/util.h"

BookVector loadBooksFromFile(const char* filename);

#endif /* BOOK_LOADER_H */