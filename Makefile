SHELL := cmd.exe
.SHELLFLAGS := /c

# Compiler and flags
CC       := C:\ProgramData\mingw64\mingw64\bin\gcc.exe
CFLAGS   := -Wall -Wextra -Werror -std=c11 -Iinclude
LDFLAGS  := 

# Directories
SRC_DIR  := src
BUILD_DIR:= build
BIN_DIR  := bin

# Target executable name
TARGET   := $(BIN_DIR)/raya.exe

# Source and object files
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Link object files into executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile C source files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create output directories if they don't exist
$(BUILD_DIR) $(BIN_DIR):
	if not exist "$@" mkdir "$@"

# Run the built application
run: all
	@.\$(BIN_DIR)\raya.exe

# Clean build artifacts
clean:
	if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)

.PHONY: all run clean
