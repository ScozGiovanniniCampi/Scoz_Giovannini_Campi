#include "messages.h"

char* reqId_to_char(requestId reqId) {
    const size_t buffer_size = sizeof(requestId);
    char buffer[buffer_size];
    for (size_t i = buffer_size - 1; i < buffer_size; i--) {
        int index = (buffer_size - 1 - i); // Calculate the index for the hex representation
        char byte = (reqId >> (index * 8)) & 0xFF; // Extract the byte
        buffer[i] = byte;
    }
    return buffer;
}