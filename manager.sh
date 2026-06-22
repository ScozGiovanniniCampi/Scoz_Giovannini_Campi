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
    SOCKET_PATHS+=("/tmp/lib_${i}.sock")
done

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
        make clean-user-state
        exit 0
        ;;
    list_books)
        build_payload 8 #OP_CODE=8 for list_books
        echo "BOOKS LIST:"
        ;;
    list_users)
        build_payload 9 #OP_CODE=9 for list_users
        echo "USERS LIST:"
        ;;
    *)
        echo "Error: Unknown operation '$OPERATION'" >&2
        exit 1
        ;;
esac


# Send payload to every library socket and capture responses (waits max 6 seconds each)
for (( i=0; i<LIBRARY_COUNT; i++ )); do
    SOCKET_PATH="${SOCKET_PATHS[i]}"
    RESPONSE=$(printf "%s" "$PAYLOAD" | nc -N -w 6 -U "$SOCKET_PATH" 2>/dev/null || true)
    if [ -z "$RESPONSE" ]; then
        echo "Error: No response from library process ${i}." >&2
    else
        IFS="$ETX" read -d "$EOT" -r -a FIELDS <<< "$RESPONSE"
        RESP_OP="${FIELDS[0]}"
        if [ "$RESP_OP" -ne 8 ] && [ "$RESP_OP" -ne 9 ]; then
            echo "Error: Unexpected response from library process ${i}." >&2
            continue
        fi
        COUNT="${FIELDS[1]}"
        if [ "$COUNT" -ne 0 ]; then
            echo "From library process ${i}:"
            for ((j=2; j<2+COUNT; j+=2)); do
                echo "- ${FIELDS[j]} [${FIELDS[j+1]}]"
            done
        fi
    fi
done
