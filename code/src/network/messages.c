#include "messages.h"

#include <stdio.h>
#include <stdlib.h>

#include "utils/util.h"

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

char* userType_to_char(UserType user_type) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", user_type);
    return buffer;
}

char* size_t_to_char(size_t value) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%zu", value);
    return buffer;
}

char* unsigned_int_to_char(unsigned int value) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u", value);
    return buffer;
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

UserType char_to_userType(const char* str) {
    int val = (int)strtol(str, NULL, 10);
    static __thread UserType user_type;
    user_type = (UserType)val;
    return user_type;
}

size_t char_to_size_t(const char* str) {
    static __thread size_t value;
    value = (size_t)strtoul(str, NULL, 10);
    return value;
}
