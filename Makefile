# Compiler selection: use GCC by default.
CC = gcc

# Compiler flags: enable warnings, treat warnings as errors, use C11, include debug symbols,
# and expose POSIX interfaces such as strnlen.
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -D_POSIX_C_SOURCE=200809L -pthread

# Linker flags: empty by default, but can be extended if needed.
LDFLAGS = -pthread

# Source and expected output locations.
SRC_DIR = code/src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TARGET = $(BUILD_DIR)/library

# Collect all C source files recursively from the source directory.
SRCS = $(shell find $(SRC_DIR) -name "*.c")
HDRS = $(shell find $(SRC_DIR) -name "*.h")

# Derive object file names from source file names, placing them in OBJ_DIR.
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default runtime arguments, overridable from the command line.
BOOKS_FILE ?= code/books.csv
NUM_LIBRARIES ?= 3
LIBRARY_ID ?= 0
TARGET_DIR ?= build

# Declare phony targets so make does not confuse them with actual files.
.PHONY: build run clean format lint

# Build target depends on the final executable.
build: $(TARGET)

# Link all object files into the executable.
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(LDFLAGS) $^ -o $@

# Compile each source file into an object file.
# The directory is created on demand before compiling.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

# Run the built program with the configured library ID, bootstrapping catalogs first.
run: build clean-sockets
	@echo "Bootstrapping $(NUM_LIBRARIES) libraries from $(BOOKS_FILE) into $(TARGET_DIR)"
	TARGET_DIR=$(TARGET_DIR) ./bootstrap.sh $(NUM_LIBRARIES) $(BOOKS_FILE)
	@for id in $$(seq 0 $$(($$(echo $(NUM_LIBRARIES)) - 1))); do \
		echo "Starting library $$id in the background with catalog=$(TARGET_DIR)/catalog$$(printf "%02d" $$id).csv"; \
		$(TARGET) $$id $(NUM_LIBRARIES) $(TARGET_DIR)/catalog$$(printf "%02d" $$id).csv & \
	done; \
	wait

# Format source and header files.
format:
	clang-format -i $(SRCS) $(HDRS)

# Lint source files using clang-tidy.
lint:
	clang-tidy $(SRCS) -- -I$(SRC_DIR) $(CFLAGS)

# Remove compiled artifacts and the executable.
clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) clean-sockets

# Remove any leftover socket files in /tmp.
clean-sockets:
	rm -f build/lib_*.sock
