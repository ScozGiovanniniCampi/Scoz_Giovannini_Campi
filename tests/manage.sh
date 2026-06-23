#!/usr/bin/env bash

# Exit immediately on error
set -euo pipefail

echo "=== Starting Manager Integration Tests ==="

# 1. Clean and Build
echo "Building the project..."
make clean
make build

# Ensure no library processes are running to start with
pkill -x library || true
make clean-sockets
make clean-user-state

# Helper functions for assertions and logging
declare -a FAILED_TESTS=()

# Log directory setup
LOG_DIR="tests/logs"
mkdir -p "$LOG_DIR"
rm -f "$LOG_DIR"/*_manager.log # Clear previous manager test logs

# Helper to normalize test name for filenames
safe_filename() {
    echo "$1" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9_-]+/_/g' | sed -E 's/^_+|_+$//g'
}

run_test_with_logging() {
    local test_name="$1"
    local expected_success="$2"
    shift 2

    local log_file="$LOG_DIR/$(safe_filename "$test_name")_manager.log"
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

# Pre-running phase, libraries are not running
NUM_LIBRARIES=0

# Test 1: Check status when no instances are running
assert_success "Status when idle" ./manage.sh status

# Test 2: Check failure of other operations when no instances are running
assert_failure "Stop when idle" ./manage.sh stop
assert_failure "List books when idle" ./manage.sh list_books
assert_failure "List users when idle" ./manage.sh list_users
assert_failure "Invalid operation" ./manage.sh invalid_op
assert_failure "No arguments" ./manage.sh
assert_failure "Too many arguments" ./manage.sh status extra

# 2. Bootstrap catalogs
echo "Bootstrapping 3 libraries..."
NUM_LIBRARIES=3
BOOKS_FILE="code/books.csv"
TARGET_DIR="build"
TARGET_DIR=$TARGET_DIR ./bootstrap.sh $NUM_LIBRARIES $BOOKS_FILE

# 3. Start library processes in the background redirecting to log files
declare -a PIDS=()
for id in $(seq 0 $((NUM_LIBRARIES - 1))); do
    echo "Starting library $id in the background..."
    stdbuf -oL build/library "$id" "$NUM_LIBRARIES" "$TARGET_DIR/catalog$(printf "%02d" "$id").csv" > "lib_${id}.log" 2>&1 &
    PIDS+=($!)
done

# Ensure we clean up background processes on exit (if they are not stopped by the test)
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

# Test 3: Check status with libraries running
assert_success "Status with 3 running libraries" ./manage.sh status

# Test 4: List books and check output
test_list_books() {
    local out
    out=$(./manage.sh list_books)
    echo "$out"
    echo "$out" | grep -q "BOOKS LIST:"
    echo "$out" | grep -q "The Great Gatsby"
}
assert_success "List books output validation" test_list_books

# Test 5: List users when there are no registered users
test_list_users_empty() {
    local out
    out=$(./manage.sh list_users)
    echo "$out"
    echo "$out" | grep -q "USERS LIST:"
    echo "$out" | grep -q "No users found in library process"
}
assert_success "List users empty validation" test_list_users_empty

# Register some users and check list_users again
echo "Registering test users..."
./user.sh Alice 0 register
./user.sh Bob 1 register

test_list_users() {
    local out
    out=$(./manage.sh list_users)
    echo "$out"
    echo "$out" | grep -q "USERS LIST:"
    echo "$out" | grep -q "\- Alice"
    echo "$out" | grep -q "\- Bob"
}
assert_success "List users populated validation" test_list_users

# Test: Check book status is initially AVAILABLE
test_book_available() {
    local out
    out=$(./manage.sh list_books)
    echo "$out"
    echo "$out" | grep -q "Pride and Prejudice \[AVAILABLE\]"
}
assert_success "Verify book is initially AVAILABLE" test_book_available

# Test: Borrow a book and check if it updates to BORROWED in book list, and shows up for the user
echo "Alice borrowing Pride and Prejudice..."
./user.sh Alice 0 borrow "Pride and Prejudice"

test_borrowed_state() {
    local books_out users_out
    books_out=$(./manage.sh list_books)
    users_out=$(./manage.sh list_users)
    echo "$books_out"
    echo "$users_out"
    echo "$books_out" | grep -q "Pride and Prejudice \[BORROWED\]"
    echo "$users_out" | grep -q "\- Alice \[Pride and Prejudice\]"
}
assert_success "Verify book is BORROWED and associated with user" test_borrowed_state

# Test: Return the book and verify it changes back to AVAILABLE, and is no longer showing as borrowed by the user
echo "Alice returning Pride and Prejudice..."
./user.sh Alice 0 return "Pride and Prejudice"

test_returned_state() {
    local books_out users_out
    books_out=$(./manage.sh list_books)
    users_out=$(./manage.sh list_users)
    echo "$books_out"
    echo "$users_out"
    echo "$books_out" | grep -q "Pride and Prejudice \[AVAILABLE\]"
    # Ensure Alice is still in the user list but does not have the book title next to her name
    echo "$users_out" | grep -q "\- Alice"
    ! echo "$users_out" | grep -q "\- Alice \[Pride and Prejudice\]"
}
assert_success "Verify book is AVAILABLE again and user has no borrowed books" test_returned_state

# Test 6: Stop command
assert_success "Stop running libraries via manager" ./manage.sh stop

# Verify they are indeed stopped
test_stopped_status() {
    local out
    out=$(./manage.sh status)
    echo "$out"
    echo "$out" | grep -q "Running instances: 0"
}
assert_success "Verify instances are stopped" test_stopped_status

# Clean up trap/pids to avoid error wait blocks on exit since they are already killed
PIDS=()

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
