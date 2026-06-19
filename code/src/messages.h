#ifndef MESSAGES_H
#define MESSAGES_H

typedef unsigned int requestId;

char* reqId_to_char(requestId reqId);

enum OperationType {
    OP_ANSWER,
    OP_REGISTER,
    OP_SEARCH,
    OP_SEARCH_RESULT,
    OP_BORROW,
    OP_RETURN,
    OP_GET_USERS,
    OP_USERS_RESULT,
    OP_GET_BOOKS,
    OP_BOOKS_RESULT,
    OP_ERROR = -1
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

// TODO: Add more error codes as needed
enum ResultCode {
    RESULT_SUCCESS,
    ERROR_USER_NOT_REGISTERED,
};

const char END_OF_ARGUMENT = 3; // ASCII ETX (End of Text) character
const char END_OF_TRANSMISSION = 4; // ASCII EOT (End of Transmission) character

#endif // MESSAGES_H
