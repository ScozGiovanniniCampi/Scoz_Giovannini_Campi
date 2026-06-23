#!/usr/bin/env bash

# Exit immediately on error
set -euo pipefail

echo "=== Starting Integration Tests ==="

# 1. Clean and Build
echo "Building the project..."
make clean
make build

# 2. Bootstrap catalogs
echo "Bootstrapping 3 libraries..."
NUM_LIBRARIES=3
BOOKS_FILE="code/books.csv"
TARGET_DIR="build"
TARGET_DIR=$TARGET_DIR ./bootstrap.sh $NUM_LIBRARIES $BOOKS_FILE

# 3. Start library processes in the background redirecting output to log files
declare -a PIDS=()
for id in $(seq 0 $((NUM_LIBRARIES - 1))); do
    echo "Starting library $id in the background..."
    stdbuf -oL build/library "$id" "$NUM_LIBRARIES" "$TARGET_DIR/catalog$(printf "%02d" "$id").csv" > "lib_${id}.log" 2>&1 &
    PIDS+=($!)
done

# Ensure we clean up background processes on exit
cleanup() {
    echo "=== Cleaning up library processes ==="
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    rm -f lib_*.log
    make clean
}
trap cleanup EXIT

# Wait a brief moment for the sockets to initialize
sleep 2

# Log directory setup
LOG_DIR="tests/logs"
mkdir -p "$LOG_DIR"
rm -f "$LOG_DIR"/*_user.log # Clear previous user test logs

# Helper to normalize test name for filenames
safe_filename() {
    echo "$1" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9_-]+/_/g' | sed -E 's/^_+|_+$//g'
}

run_test_with_logging() {
    local test_name="$1"
    local expected_success="$2"
    shift 2

    local log_file="$LOG_DIR/$(safe_filename "$test_name")_user.log"
    echo "==========================================" > "$log_file"
    echo "Test: $test_name" >> "$log_file"
    echo "Command: $*" >> "$log_file"
    echo "Expected: $([ "$expected_success" = "true" ] && echo "Success" || echo "Failure")" >> "$log_file"
    echo "==========================================" >> "$log_file"

    # Mark start in library logs
    local num_libs=${NUM_LIBRARIES:-0}
    if [ "$num_libs" -gt 0 ]; then
        for id in $(seq 0 $((num_libs - 1))); do
            echo "--- TEST START: $test_name ---" >> "lib_${id}.log"
        done
    fi

    # Run the command and capture output
    local exit_code=0
    local cmd_output
    cmd_output=$("$@" 2>&1) || exit_code=$?

    # Mark end in library logs
    if [ "$num_libs" -gt 0 ]; then
        for id in $(seq 0 $((num_libs - 1))); do
            echo "--- TEST END: $test_name ---" >> "lib_${id}.log"
        done
    fi

    echo -e "\n--- Script Output ---" >> "$log_file"
    echo "$cmd_output" >> "$log_file"
    echo "Exit Code: $exit_code" >> "$log_file"

    # Extract library log sections
    if [ "$num_libs" -gt 0 ]; then
        for id in $(seq 0 $((num_libs - 1))); do
            echo -e "\n--- Library $id Output ---" >> "$log_file"
            if [ -f "lib_${id}.log" ]; then
                # Extract lines between the marks
                sed -n "/--- TEST START: $test_name ---/,/--- TEST END: $test_name ---/p" "lib_${id}.log" \
                    | grep -v -- "--- TEST START:" | grep -v -- "--- TEST END:" >> "$log_file" || true
            else
                echo "(No library log file found)" >> "$log_file"
            fi
        done
    fi

    # Determine pass/fail
    local passed=false
    if [ "$expected_success" = "true" ] && [ $exit_code -eq 0 ]; then
        passed=true
    elif [ "$expected_success" = "false" ] && [ $exit_code -ne 0 ]; then
        passed=true
    fi

    if [ "$passed" = "true" ]; then
        echo "PASS" >> "$log_file"
        return 0
    else
        echo "FAIL" >> "$log_file"
        return 1
    fi
}

# 4. Run Test Cases
set +e

# Keep track of failures
declare -a FAILED_TESTS=()

assert_success() {
    local test_name="$1"
    shift
    echo -n "Running: $test_name ... "
    if ! run_test_with_logging "$test_name" "true" "$@"; then
        FAILED_TESTS+=("$test_name")
        echo "FAIL" >&2
    else
        echo "PASS"
    fi
}

assert_failure() {
    local test_name="$1"
    shift
    echo -n "Running: $test_name (Expecting Failure) ... "
    if ! run_test_with_logging "$test_name" "false" "$@"; then
        FAILED_TESTS+=("$test_name")
        echo "FAIL" >&2
    else
        echo "PASS"
    fi
}

assert_success "Register Alice on Library 0" ./user.sh Alice 0 register
assert_success "Register Bob on Library 1" ./user.sh Bob 1 register
assert_success "Register Charlie on Library 2" ./user.sh Charlie 2 register

assert_success "Search by title Pride" ./user.sh Alice 0 search --by title "Pride"
assert_success "Search by author Orwell" ./user.sh Bob 1 search --by author "Orwell"
assert_success "Search by year 1984" ./user.sh Charlie 2 search --by year 1984

assert_success "Borrow Pride and Prejudice (Local)" ./user.sh Alice 0 borrow "Pride and Prejudice"
assert_success "Borrow Dune (Remote)" ./user.sh Bob 1 borrow "Dune"

assert_failure "Double borrow Dune" ./user.sh Charlie 2 borrow "Dune"

assert_success "Return Pride and Prejudice" ./user.sh Alice 0 return "Pride and Prejudice"
assert_success "Return Dune" ./user.sh Bob 1 return "Dune"

assert_failure "Double register Alice" ./user.sh Alice 0 register

assert_failure "Borrow without registering" ./user.sh Dave 0 borrow "Animal Farm"

assert_success "Borrow Dune for multiple books test setup" ./user.sh Bob 1 borrow "Dune"
assert_failure "Borrow second book (1984) when already borrowing" ./user.sh Bob 1 borrow "1984"
assert_success "Return Dune after multiple books test" ./user.sh Bob 1 return "Dune"

assert_failure "Return book not borrowed" ./user.sh Alice 0 return "Animal Farm"

assert_success "Borrow Dune for wrong library return test setup" ./user.sh Bob 1 borrow "Dune"
assert_failure "Return Dune to wrong library" ./user.sh Bob 2 return "Dune"
assert_success "Return Dune to correct library" ./user.sh Bob 1 return "Dune"

# Command line argument edge cases
assert_failure "Too few arguments (none)" ./user.sh
assert_failure "Too few arguments (one)" ./user.sh Alice
assert_failure "Too few arguments (two)" ./user.sh Alice 0
assert_failure "Borrow missing title" ./user.sh Alice 0 borrow
assert_failure "Return missing title" ./user.sh Alice 0 return
assert_failure "Search missing by option" ./user.sh Alice 0 search
assert_failure "Search invalid by option" ./user.sh Alice 0 search --by publisher "O'Reilly"
assert_failure "Search missing term" ./user.sh Alice 0 search --by title
assert_failure "Invalid operation" ./user.sh Alice 0 invalid_operation

# Socket edge cases
assert_failure "Non-existent library socket" ./user.sh Alice 99 register

# Print summary at the end
echo -e "\n=== Test Execution Summary ==="
if [ ${#FAILED_TESTS[@]} -eq 0 ]; then
    echo "ALL TESTS PASSED!"
    exit 0
else
    echo "THE FOLLOWING TESTS FAILED:"
    for ft in "${FAILED_TESTS[@]}"; do
        echo "  - $ft"
    done
    exit 1
fi
