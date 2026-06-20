#include "messages.h"

#include <stdio.h>
#include <stdlib.h>

#include "utils/util.h"

char* reqId_to_char(requestId reqId) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u", reqId);
    return buffer;
}

char* resultCode_to_char(ResultCode result_code) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", result_code);
    return buffer;
}

char* operationType_to_char(OperationType op_type) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", op_type);
    return buffer;
}

char* searchType_to_char(SearchType search_type) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", search_type);
    return buffer;
}

char* senderType_to_char(SenderType sender_type) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", sender_type);
    return buffer;
}

char* size_t_to_char(size_t value) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%zu", value);
    return buffer;
}

requestId char_to_reqId(const char* str) {
    static __thread requestId reqId;
    reqId = (requestId)strtoul(str, NULL, 10);
    return reqId;
}

ResultCode char_to_resultCode(const char* str) {
    int val = (int)strtol(str, NULL, 10);
    static __thread ResultCode result_code;
    result_code = (ResultCode)val;
    return result_code;
}

OperationType char_to_operationType(const char* str) {
    int val = (int)strtol(str, NULL, 10);
    static __thread OperationType op_type;
    op_type = (OperationType)val;
    return op_type;
}

SearchType char_to_searchType(const char* str) {
    int val = (int)strtol(str, NULL, 10);
    static __thread SearchType search_type;
    search_type = (SearchType)val;
    return search_type;
}

SenderType char_to_senderType(const char* str) {
    int val = (int)strtol(str, NULL, 10);
    static __thread SenderType sender_type;
    sender_type = (SenderType)val;
    return sender_type;
}

size_t char_to_size_t(const char* str) {
    static __thread size_t value;
    value = (size_t)strtoul(str, NULL, 10);
    return value;
}
