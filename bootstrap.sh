#!/usr/bin/env bash

# Exit immediately on error, treat unset variables as errors
set -euo pipefail

# Print usage if insufficient arguments
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <lib_number> <file>" >&2
    exit 1
fi

LIB_NUMBER="$1"
FILE="$2"
TARGET_DIR="${TARGET_DIR:-.}"

# Verify that lib_number is a positive integer
if [[ ! "$LIB_NUMBER" =~ ^[0-9]+$ ]] || [ "$LIB_NUMBER" -le 0 ]; then
    echo "Error: lib_number must be a positive integer." >&2
    exit 1
fi

# Verify file exists
if [ ! -f "$FILE" ]; then
    echo "Error: Catalog file '${FILE}' does not exist or is not a regular file." >&2
    exit 1
fi

# Ensure target directory exists
mkdir -p "$TARGET_DIR"

tail -n +2 "$FILE" > "$TARGET_DIR/catalog0.csv"
split -n l/"$LIB_NUMBER" -d --additional-suffix=".csv" "$TARGET_DIR/catalog0.csv" "$TARGET_DIR/catalog"
rm "$TARGET_DIR/catalog0.csv"
