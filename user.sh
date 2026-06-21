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

# Delimiters
ETX=$'\x03' # End of Text (Argument delimiter)
EOT=$'\x04' # End of Transmission

# Helper function to construct the payload by joining arguments with ETX and appending EOT
build_payload() {
    local IFS="$ETX"
    PAYLOAD="$*${ETX}${EOT}"
}

# Generate/increment a persistent counter per username safely using flock
STATE_DIR="/tmp/lib_user_state"
mkdir -p "$STATE_DIR"
COUNTER_FILE="${STATE_DIR}/${USERNAME}_counter.txt"
LOCK_FILE="${STATE_DIR}/${USERNAME}_counter.lock"

# Open the lock file and acquire an exclusive lock (blocks until available)
exec 9>"$LOCK_FILE"
flock -x 9

if [ -f "$COUNTER_FILE" ]; then
    REQ_ID=$(cat "$COUNTER_FILE")
    # Validate it is an integer, fallback to 0 if not
    if [[ ! "$REQ_ID" =~ ^[0-9]+$ ]]; then
        REQ_ID=0
    fi
    REQ_ID=$((REQ_ID + 1))
else
    REQ_ID=1
fi
echo "$REQ_ID" > "$COUNTER_FILE"

# Release the lock and close file descriptor 9
flock -u 9
exec 9>&-

# Build payload based on operation
case "$OPERATION" in
    register)
        # Payload: OP_REGISTER (1) | REQ_ID | USERNAME
        build_payload 1 "$REQ_ID" "$USERNAME"
        ;;
    borrow)
        if [ "$#" -lt 4 ]; then
            echo "Error: Missing book title. Usage: $0 <username> <library_id> borrow \"<book_title>\"" >&2
            exit 1
        fi
        BOOK_TITLE="$4"
        # Payload: OP_BORROW (4) | REQ_ID | SENDER_USER (0) | USERNAME | BOOK_TITLE
        build_payload 4 "$REQ_ID" 0 "$USERNAME" "$BOOK_TITLE"
        ;;
    return)
        if [ "$#" -lt 4 ]; then
            echo "Error: Missing book title. Usage: $0 <username> <library_id> return \"<book_title>\"" >&2
            exit 1
        fi
        BOOK_TITLE="$4"
        # Payload: OP_RETURN (5) | REQ_ID | SENDER_USER (0) | USERNAME | BOOK_TITLE
        build_payload 5 "$REQ_ID" 0 "$USERNAME" "$BOOK_TITLE"
        ;;
    search)
        if [ "$#" -lt 6 ] || [ "$4" != "--by" ]; then
            echo "Error: Invalid arguments. Usage: $0 <username> <library_id> search --by <title|author|year> <term>" >&2
            exit 1
        fi
        SEARCH_BY="$5"
        SEARCH_TERM="$6"
        case "$SEARCH_BY" in
            title)  SEARCH_TYPE=0 ;;
            author) SEARCH_TYPE=1 ;;
            year)   SEARCH_TYPE=2 ;;
            *)
                echo "Error: Invalid search type '$SEARCH_BY'. Must be title, author, or year." >&2
                exit 1
                ;;
        esac
        # Payload: OP_SEARCH (2) | REQ_ID | SENDER_USER (0) | SEARCH_TYPE | SEARCH_TERM
        build_payload 2 "$REQ_ID" 0 "$SEARCH_TYPE" "$SEARCH_TERM"
        ;;
    *)
        echo "Error: Unknown operation '$OPERATION'. Must be register, borrow, return, or search." >&2
        exit 1
        ;;
esac

# TODO: decrease wait when search and borrow execute in parallel
# Send payload to library process and capture response (waits max 120 seconds)
RESPONSE=$(printf "%s" "$PAYLOAD" | nc -N -w 120 -U "$SOCKET_PATH" 2>/dev/null || true)

if [ -z "$RESPONSE" ]; then
    echo "Error: No response from the library process at '${SOCKET_PATH}'." >&2
    exit 1
fi

# Parse response (split fields on ETX, stripping EOT)
# Note: The response is formatted as RESP_OP \x03 REQ_ID \x03 RESULT_CODE \x03 \x04
IFS="$ETX" read -d "$EOT" -r -a FIELDS <<< "$RESPONSE"

# TODO: fix search result by interpreting in a different way (3 [request_id]  <book_count> <books_vector> <books_vector>)

RESP_OP="${FIELDS[0]}"
RESP_REQ_ID="${FIELDS[1]}"
RESULT_CODE="${FIELDS[2]}"

# Ensure response matches our request ID
if [ "$RESP_REQ_ID" -ne "$REQ_ID" ]; then
    echo "Error: Request ID mismatch. Expected $REQ_ID, got $RESP_REQ_ID." >&2
    exit 1
fi

# Handle the result code
case "$RESULT_CODE" in
    0)
        echo "Success: '$OPERATION' completed successfully."
        exit 0
        ;;
    1)
        echo "Error: '$OPERATION' failed." >&2
        exit 1
        ;;
    2)
        echo "Error: User '$USERNAME' is not registered with this library." >&2
        exit 1
        ;;
    3)
        echo "Error: Book not found." >&2
        exit 1
        ;;
    4)
        echo "Error: Book is already borrowed." >&2
        exit 1
        ;;
    *)
        echo "Response: Received code $RESULT_CODE."
        exit 0
        ;;
esac
