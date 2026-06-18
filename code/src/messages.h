#ifndef MESSAGES_H
#define MESSAGES_H

enum OperationType {
    OP_ANSWER,
    OP_REGISTER,
    OP_SEARCH,
    OP_SEARCH_RESULT,
    OP_BORROW,
    OP_BORROW_RESULT,
    OP_RETURN,
    OP_GET_USERS,
    OP_GET_BOOKS,
};

enum SearchType {
    SEARCH_BY_TITLE,
    SEARCH_BY_AUTHOR,
    SEARCH_BY_YEAR,
};

enum SenderType {
    SENDER_USER,
    SENDER_LIBRARY,
    SENDER_MANAGER,
};

enum ErrorCode {
    ERROR_NONE,
    ERROR_USER_REGISTERED,
    ERROR_USER_NOT_REGISTERED,
    ERROR_BOOK_NOT_FOUND,
    ERROR_BOOK_UNAVAILABLE,
    ERROR_INVALID_OPERATION,
};

const char END_OF_ARGUMENT = 3; // ASCII ETX (End of Text) character
const char END_OF_TRANSMISSION = 4; // ASCII EOT (End of Transmission) character

#endif // MESSAGES_H
