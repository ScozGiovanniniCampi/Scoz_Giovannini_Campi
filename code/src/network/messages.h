#ifndef MESSAGES_H
#define MESSAGES_H

#include <stddef.h>

typedef enum {
    OP_ANSWER = 0,
    OP_REGISTER = 1,
    OP_SEARCH = 2,
    OP_SEARCH_RESULT = 3,
    OP_BORROW = 4,
    OP_RETURN = 5,
    OP_GET_USERS = 6,
    OP_USERS_RESULT = 7,
    OP_GET_BOOKS = 8,
    OP_BOOKS_RESULT = 9
} OperationType;

typedef enum {
    SEARCH_BY_TITLE,
    SEARCH_BY_AUTHOR,
    SEARCH_BY_YEAR,
} SearchType;

typedef enum {
    USER_USER,
    USER_LIBRARY,
    USER_MANAGER,
} UserType;

typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_FAILURE = 1,
    ERROR_USER_NOT_REGISTERED = 2,
    ERROR_BOOK_NOT_FOUND = 3,
    ERROR_BOOK_ALREADY_BORROWED = 4,
    ERROR_USER_ALREADY_REGISTERED = 5,
    ERROR_USER_ALREADY_BORROWED_BOOK = 6,
    ERROR_BOOK_NOT_BORROWED = 7,
    ERROR_BOOK_NOT_BORROWED_BY_USER = 8,
    ERROR_USER_HAS_NO_BORROWED_BOOK = 9,
} ResultCode;

char* resultCode_to_char(ResultCode result_code);
char* operationType_to_char(OperationType op_type);
char* searchType_to_char(SearchType search_type);
char* userType_to_char(UserType user_type);
char* size_t_to_char(size_t value);
char* unsigned_int_to_char(unsigned int value);
ResultCode char_to_resultCode(const char* str);
OperationType char_to_operationType(const char* str);
SearchType char_to_searchType(const char* str);
UserType char_to_userType(const char* str);
size_t char_to_size_t(const char* str);

#define END_OF_ARGUMENT 3      // ASCII ETX (End of Text) character
#define END_OF_TRANSMISSION 4  // ASCII EOT (End of Transmission) character

#endif  // MESSAGES_H
