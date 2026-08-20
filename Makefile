# Compiler and flags
CC       := C:\ProgramData\mingw64\mingw64\bin\gcc.exe
CFLAGS = -Wall -Wextra -Werror -std=c11 -O2 -Isrc
LDFLAGS =

SRCDIR = src
OBJDIR = obj
BINDIR = bin
TESTDIR := tests

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET = $(BINDIR)/raya

.PHONY: all clean test dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

$(TARGET): $(OBJECTS)
	@echo "  LINK    $@"
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

test: test-lexer test-parser

test-lexer: all
	@echo "Running lexer tests..."
	@set -e; \
	for f in tests/lexer/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="tests/lexer/$$name.expected"; \
		echo "  $$f"; \
		if [ ! -f "$$expected" ]; then \
			echo "    FAIL: missing $$expected"; \
			exit 1; \
		fi; \
		$(TARGET) --dump-tokens "$$f" 2>/dev/null | \
			awk 'BEGIN { found=0 } \
			/^KIND[[:space:]]+TEXT/ { found=1; next } \
			/^----/ { next } \
			/^Total:/ { exit } \
			found && $$1 != "" { print $$1 }' \
			> "$$expected.actual"; \
		if diff -u "$$expected" "$$expected.actual"; then \
			echo "    PASS"; \
		else \
			echo "    FAIL"; \
			rm -f "$$expected.actual"; \
			exit 1; \
		fi; \
		rm -f "$$expected.actual"; \
	done

test-parser: all
	@echo "Running parser tests..."
	@set -e; \
	for f in tests/parser/*.raya; do \
		if [ ! -f "$$f" ]; then continue; fi; \
		name=$$(basename "$$f" .raya); \
		expected="tests/parser/$$name.expected"; \
		echo "  $$f"; \
		if [ ! -f "$$expected" ]; then \
			echo "    FAIL: missing $$expected"; \
			exit 1; \
		fi; \
		$(TARGET) --dump-ast "$$f" 2>/dev/null | \
			sed -n '/=== AST DUMP ===/,$${/=== AST DUMP ===/d; p}' \
			> "$$expected.actual"; \
		if diff -u "$$expected" "$$expected.actual"; then \
			echo "    PASS"; \
		else \
			echo "    FAIL"; \
			rm -f "$$expected.actual"; \
			exit 1; \
		fi; \
		rm -f "$$expected.actual"; \
	done

debug: CFLAGS = -Wall -Wextra -std=c11 -g -fsanitize=address -Isrc
debug: LDFLAGS = -fsanitize=address
debug: all
