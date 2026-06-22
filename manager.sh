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

RUNNING_LIBRARIES=$(pgrep -f "${LIB_PATH}" || true)
if [ -z "$RUNNING_LIBRARIES" ]; then
    LIBRARY_COUNT=0
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


# TODO: decrease wait when search and borrow execute in parallel
# Send payload to every library socket and capture responses (waits max 120 seconds each)
for socket_path in "${SOCKET_PATHS[@]}"; do
    response=$(printf "%s" "$PAYLOAD" | nc -N -w 120 -U "$socket_path" 2>/dev/null || true)
    if [ -z "$response" ]; then
        echo "Error: No response from a library process at '${socket_path}'." >&2
    else
        IFS="$ETX" read -d "$EOT" -r -a FIELDS <<< "$response"
        RESP_OP="${FIELDS[0]}"
        if [ "$RESP_OP" -ne 8 ] && [ "$RESP_OP" -ne 9 ]; then
            echo "Error: Unexpected response from library process at '${socket_path}'." >&2
            continue
        fi
        COUNT="${FIELDS[1]}"
        if [ "$COUNT" -ne 0 ]; then
            for ((i=2; i<2+COUNT; i++)); do
                echo "- ${FIELDS[i]}"
            done
        fi
    fi
done
