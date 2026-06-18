# Compiler selection: use GCC by default.
CC = gcc

# Compiler flags: enable warnings, treat warnings as errors, use C11, include debug symbols,
# and expose POSIX interfaces such as strnlen.
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -D_POSIX_C_SOURCE=200809L

# Linker flags: empty by default, but can be extended if needed.
LDFLAGS =

# Source and expected output locations.
SRC_DIR = code/src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TARGET = $(BUILD_DIR)/library

# Collect all C source files from the source directory.
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Derive object file names from source file names, placing them in OBJ_DIR.
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default runtime arguments, overridable from the command line.
BOOKS_FILE ?= code/books.csv
LIBRARY_ID ?= 0

# Declare phony targets so make does not confuse them with actual files.
.PHONY: build run clean

# Build target depends on the final executable.
build: $(TARGET)

# Link all object files into the executable.
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(LDFLAGS) $^ -o $@

# Compile each source file into an object file.
# The directory is created on demand before compiling.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

# Run the built program with the configured library ID and books file.
run: build
	@echo "Running $(TARGET) with ID=$(LIBRARY_ID) and books=$(BOOKS_FILE)"
	$(TARGET) $(LIBRARY_ID) $(BOOKS_FILE)

# Remove compiled artifacts and the executable.
clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/lib_*.sock
