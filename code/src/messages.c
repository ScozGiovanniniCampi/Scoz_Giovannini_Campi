#include "messages.h"
#include <stdio.h>

char* reqId_to_char(requestId reqId) {
    static __thread char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u", reqId);
    return buffer;
}