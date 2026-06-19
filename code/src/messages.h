#ifndef MESSAGES_H
#define MESSAGES_H

typedef unsigned int requestId;


typedef enum {
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
} OperationType;

typedef enum {
    SEARCH_BY_TITLE,
    SEARCH_BY_AUTHOR,
    SEARCH_BY_YEAR,
} SearchType;

typedef enum {
    SENDER_USER,
    SENDER_LIBRARY,
    SENDER_MANAGER,
} SenderType;

// TODO: Add more error codes as needed
typedef enum {
    RESULT_SUCCESS,
    RESULT_FAILURE,
    ERROR_USER_NOT_REGISTERED,
} ResultCode;


char* reqId_to_char(requestId reqId);
char* resultCode_to_char(ResultCode result_code);
char *operationType_to_char(OperationType op_type);
char *searchType_to_char(SearchType search_type);
char *senderType_to_char(SenderType sender_type);
char* size_t_to_char(size_t value);
requestId char_to_reqId(const char* str);
ResultCode char_to_resultCode(const char* str);
OperationType char_to_operationType(const char* str);
SearchType char_to_searchType(const char* str);
SenderType char_to_senderType(const char* str);
size_t char_to_size_t(const char* str);


#define END_OF_ARGUMENT 3 // ASCII ETX (End of Text) character
#define END_OF_TRANSMISSION 4 // ASCII EOT (End of Transmission) character

#endif // MESSAGES_H
