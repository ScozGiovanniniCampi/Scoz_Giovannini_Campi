#!/usr/bin/env bash

# Exit immediately on error, treat unset variables as errors
set -euo pipefail

# Print usage if insufficient arguments
if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <username> <library_id> <operation> [operation_args]" >&2
    echo "Operations: register, borrow, return, search" >&2
    exit 1
fi

USERNAME="$1"
LIBRARY_ID="$2"
OPERATION="$3"
SOCKET_PATH="/tmp/lib_${LIBRARY_ID}.sock"

# Verify socket exists
if [ ! -S "$SOCKET_PATH" ]; then
    echo "Error: Library socket '${SOCKET_PATH}' does not exist. Is the library process running?" >&2
    exit 1
fi

# Constants (operation codes, search types, result codes, timeouts)
OP_REGISTER=1
OP_SEARCH=2
OP_SEARCH_RESULT=3
OP_BORROW=4
OP_RETURN=5
SENDER_USER=0

SEARCH_BY_TITLE=0
SEARCH_BY_AUTHOR=1
SEARCH_BY_YEAR=2

RESULT_SUCCESS=0
RESULT_FAILURE=1
RESULT_NOT_REGISTERED=2
RESULT_BOOK_NOT_FOUND=3
RESULT_ALREADY_BORROWED=4
RESULT_USER_ALREADY_REGISTERED=5
RESULT_USER_ALREADY_BORROWED=6
RESULT_BOOK_NOT_BORROWED=7
RESULT_NOT_BORROWED_BY_USER=8
RESULT_NO_BORROWED_BOOKS=9

NC_TIMEOUT=11

ETX=$'\x03' # End of Text (Argument delimiter)
EOT=$'\x04' # End of Transmission

# Helper function to construct the payload by joining arguments with ETX and appending EOT
build_payload() {
    local IFS="$ETX"
    PAYLOAD="$*${ETX}${EOT}"
}

# Build payload based on operation
case "$OPERATION" in
    register)
        # Payload: OP_REGISTER | USERNAME
        build_payload "$OP_REGISTER" "$USERNAME"
        ;;
    borrow)
        if [ "$#" -lt 4 ]; then
            echo "Error: Missing book title. Usage: $0 <username> <library_id> borrow \"<book_title>\"" >&2
            exit 1
        fi
        BOOK_TITLE="$4"
        # Payload: OP_BORROW | SENDER_USER | USERNAME | BOOK_TITLE
        build_payload "$OP_BORROW" "$SENDER_USER" "$USERNAME" "$BOOK_TITLE"
        ;;
    return)
        if [ "$#" -lt 4 ]; then
            echo "Error: Missing book title. Usage: $0 <username> <library_id> return \"<book_title>\"" >&2
            exit 1
        fi
        BOOK_TITLE="$4"
        # Payload: OP_RETURN | SENDER_USER | USERNAME | BOOK_TITLE
        build_payload "$OP_RETURN" "$SENDER_USER" "$USERNAME" "$BOOK_TITLE"
        ;;
    search)
        if [ "$#" -lt 6 ] || [ "$4" != "--by" ]; then
            echo "Error: Invalid arguments. Usage: $0 <username> <library_id> search --by <title|author|year> <term>" >&2
            exit 1
        fi
        SEARCH_BY="$5"
        SEARCH_TERM="$6"
        case "$SEARCH_BY" in
            title)  SEARCH_TYPE="$SEARCH_BY_TITLE" ;;
            author) SEARCH_TYPE="$SEARCH_BY_AUTHOR" ;;
            year)   SEARCH_TYPE="$SEARCH_BY_YEAR" ;;
            *)
                echo "Error: Invalid search type '$SEARCH_BY'. Must be title, author, or year." >&2
                exit 1
                ;;
        esac
        # Payload: OP_SEARCH | SENDER_USER | SEARCH_TYPE | SEARCH_TERM
        build_payload "$OP_SEARCH" "$SENDER_USER" "$SEARCH_TYPE" "$SEARCH_TERM"
        ;;
    *)
        echo "Error: Unknown operation '$OPERATION'. Must be register, borrow, return, or search." >&2
        exit 1
        ;;
esac

# Send payload to library process and capture response (waits max ${NC_TIMEOUT} seconds)
RESPONSE=$(printf "%s" "$PAYLOAD" | nc -N -w "$NC_TIMEOUT" -U "$SOCKET_PATH" 2>/dev/null || true)

if [ -z "$RESPONSE" ]; then
    echo "Error: No response from the library process at '${SOCKET_PATH}'." >&2
    exit 1
fi

# Parse response (split fields on ETX, stripping EOT)
# Note: The response is formatted as RESP_OP \x03 RESULT_CODE \x03 \x04
# Or for search: RESP_OP \x03 BOOK_COUNT \x03 BOOK_TITLE_1 \x03 ...
IFS="$ETX" read -d "$EOT" -r -a FIELDS <<< "$RESPONSE"

RESP_OP="${FIELDS[0]}"

# Interpret search results (OP_SEARCH_RESULT)
if [ "$RESP_OP" -eq "$OP_SEARCH_RESULT" ]; then
    BOOK_COUNT="${FIELDS[1]}"
    if [ "$BOOK_COUNT" -eq 0 ]; then
        echo "No books found matching search criteria."
    else
        echo "Search results ($BOOK_COUNT book(s) found):"
        for ((i=2; i<2+BOOK_COUNT; i++)); do
            echo "- ${FIELDS[i]}"
        done
    fi
    exit 0
fi

RESULT_CODE="${FIELDS[1]}"

# Handle the result code
case "$RESULT_CODE" in
    "$RESULT_SUCCESS")
        echo "Success: '$OPERATION' completed successfully."
        exit 0
        ;;
    "$RESULT_FAILURE")
        echo "Error: '$OPERATION' failed." >&2
        exit 1
        ;;
    "$RESULT_NOT_REGISTERED")
        echo "Error: User '$USERNAME' is not registered with this library." >&2
        exit 1
        ;;
    "$RESULT_BOOK_NOT_FOUND")
        echo "Error: Book not found." >&2
        exit 1
        ;;
    "$RESULT_ALREADY_BORROWED")
        echo "Error: Book is already borrowed." >&2
        exit 1
        ;;
    "$RESULT_USER_ALREADY_REGISTERED")
        echo "Error: User '$USERNAME' is already registered." >&2
        exit 1
        ;;
    "$RESULT_USER_ALREADY_BORROWED")
        echo "Error: User '$USERNAME' has already borrowed a book." >&2
        exit 1
        ;;
    "$RESULT_BOOK_NOT_BORROWED")
        echo "Error: Book is not borrowed." >&2
        exit 1
        ;;
    "$RESULT_NOT_BORROWED_BY_USER")
        echo "Error: Book is not borrowed by user '$USERNAME'." >&2
        exit 1
        ;;
    "$RESULT_NO_BORROWED_BOOKS")
        echo "Error: User '$USERNAME' has no borrowed books." >&2
        exit 1
        ;;
    *)
        echo "Response: Received code $RESULT_CODE."
        exit 0
        ;;
esac
