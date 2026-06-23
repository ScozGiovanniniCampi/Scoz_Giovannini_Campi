#!/usr/bin/env bash

# Exit immediately on error, treat unset variables as errors
set -euo pipefail

# Print usage if insufficient arguments
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <operation>" >&2
    echo "Operations: status, stop, list_books, list_users" >&2
    exit 1
fi


OPERATION="$1"
LIB_PATH="./build/library"

RUNNING_LIBRARIES=$(pgrep -x "$(basename "${LIB_PATH}")" || true)
if [ -z "$RUNNING_LIBRARIES" ]; then
    if [ "$OPERATION" = "status" ]; then
        echo "Running instances: 0"
        exit 0
    fi
    echo "Error: No running library processes found." >&2
    exit 1
else
    LIBRARY_COUNT=$(printf '%s\n' "$RUNNING_LIBRARIES" | wc -l)
fi

SOCKET_PATHS=()
for (( i=0; i<LIBRARY_COUNT; i++ )); do
    SOCKET_PATHS+=("build/lib_${i}.sock")
done

# Constants (operation codes, response codes, timeout)
OP_LIST_USERS=6
OP_LIST_BOOKS=8
RESP_USERS=7
RESP_BOOKS=9
NC_TIMEOUT=6


# Delimiters
ETX=$'\x03' # End of Text (Argument delimiter)
EOT=$'\x04' # End of Transmission

# Helper function to construct the payload by joining arguments with ETX and appending EOT
build_payload() {
    local IFS="$ETX"
    PAYLOAD="$*${ETX}${EOT}"
}


case "$OPERATION" in
    status)
        echo "Running instances: $LIBRARY_COUNT"
        exit 0
        ;;
    stop)
        if [ -n "$RUNNING_LIBRARIES" ]; then
            kill ${RUNNING_LIBRARIES}
        fi
        make clean-sockets
        exit 0
        ;;
    list_books)
        build_payload "$OP_LIST_BOOKS" #OP_CODE for list_books
        echo "BOOKS LIST:"
        ;;
    list_users)
        build_payload "$OP_LIST_USERS" #OP_CODE for list_users
        echo "USERS LIST:"
        ;;
    *)
        echo "Error: Unknown operation '$OPERATION'" >&2
        exit 1
        ;;
esac


# Send payload to every library socket and capture responses (waits max ${NC_TIMEOUT} seconds each)
for (( i=0; i<LIBRARY_COUNT; i++ )); do
    SOCKET_PATH="${SOCKET_PATHS[i]}"
    RESPONSE=$(printf "%s" "$PAYLOAD" | nc -N -w "$NC_TIMEOUT" -U "$SOCKET_PATH" 2>/dev/null || true)
    if [ -z "$RESPONSE" ]; then
        echo "Error: No response from library process ${i}." >&2
    else
        IFS="$ETX" read -d "$EOT" -r -a FIELDS <<< "$RESPONSE"
        RESP_OP="${FIELDS[0]}"
        if [ "$RESP_OP" -ne "$RESP_USERS" ] && [ "$RESP_OP" -ne "$RESP_BOOKS" ]; then
            echo "Error: Unexpected response from library process ${i}." >&2
            continue
        fi
        COUNT="${FIELDS[1]}"
        if [ "$COUNT" -ne 0 ]; then
            echo "From library process ${i}:"
            for ((j=2; j<2+COUNT*2; j+=2)); do
                if [ "$RESP_OP" -eq "$RESP_BOOKS" ]; then
                    echo "- ${FIELDS[j]} [${FIELDS[j+1]}]"
                fi
                if [ "$RESP_OP" -eq "$RESP_USERS" ]; then
                    if [ "${FIELDS[j+1]}" = "None" ]; then
                        echo "- ${FIELDS[j]}"
                    else
                        echo "- ${FIELDS[j]} [${FIELDS[j+1]}]"
                    fi
                fi
            done
        fi
        if [[ "$COUNT" -eq 0 && "$RESP_OP" -eq "$RESP_BOOKS" ]]; then
            echo "No books found in library process ${i}."
        elif [[ "$COUNT" -eq 0 && "$RESP_OP" -eq "$RESP_USERS" ]]; then
            echo "No users found in library process ${i}."
        fi
    fi
done
