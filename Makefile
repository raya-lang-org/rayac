
# Compiler and flags
CC       := gcc
SHELL    := /bin/bash
CFLAGS   := -Wall -Wextra -Werror -std=c11 -O2 -Isrc
LDFLAGS  :=

TESTDIR  := tests

SRCDIR   := src
OBJDIR   := obj
BINDIR   := bin


SOURCES  := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET   := $(BINDIR)/raya

.PHONY: all clean test test-lexer test-parser test-sema dirs debug

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

# ============================================================================
# Lexer tests
# ============================================================================

test-lexer: all
	@echo "Running lexer tests..."
	@passed=0; failed=0; \
	for f in $(TESTDIR)/lexer/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="$(TESTDIR)/lexer/$$name.expected"; \
		actual="$(TESTDIR)/lexer/$$name.actual"; \
		if [ ! -f "$$expected" ]; then \
			echo "  SKIP  $$f (no .expected)"; \
			continue; \
		fi; \
		$(TARGET) --test-lexer "$$f" > "$$actual" 2>/dev/null || true; \
		if tr -d '\r' < "$$expected" | cmp -s - <(tr -d '\r' < "$$actual"); then \
			echo "  PASS  $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo "  FAIL  $$f"; \
			echo "  diff expected actual:"; \
			diff -u \
				<(tr -d '\r' < "$$expected") \
				<(tr -d '\r' < "$$actual") || true; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo "Lexer: $$passed passed, $$failed failed"; \
	test $$failed -eq 0

# ============================================================================
# Parser tests
# ============================================================================

test-parser: all
	@echo "Running parser tests..."
	@passed=0; failed=0; \
	for f in $(TESTDIR)/parser/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="$(TESTDIR)/parser/$$name.expected"; \
		actual="$(TESTDIR)/parser/$$name.actual"; \
		if [ ! -f "$$expected" ]; then \
			echo "  SKIP  $$f (no .expected)"; \
			continue; \
		fi; \
		$(TARGET) --test-parser "$$f" > "$$actual" 2>/dev/null || true; \
		if tr -d '\r' < "$$expected" | cmp -s - <(tr -d '\r' < "$$actual"); then \
			echo "  PASS  $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo "  FAIL  $$f"; \
			echo "  diff expected actual:"; \
			diff -u \
				<(tr -d '\r' < "$$expected") \
				<(tr -d '\r' < "$$actual") || true; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo "Parser: $$passed passed, $$failed failed"; \
	test $$failed -eq 0

# ============================================================================
# Semantic analysis tests
# ============================================================================

test-sema: all
	@echo "Running sema tests..."
	@passed=0; failed=0; \
	for f in $(TESTDIR)/sema/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="$(TESTDIR)/sema/$$name.expected"; \
		actual="$(TESTDIR)/sema/$$name.actual"; \
		if [ ! -f "$$expected" ]; then \
			echo "  SKIP  $$f (no .expected)"; \
			continue; \
		fi; \
		$(TARGET) --test-sema "$$f" > "$$actual" 2>/dev/null || true; \
		if tr -d '\r' < "$$expected" | cmp -s - <(tr -d '\r' < "$$actual"); then \
			echo "  PASS  $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo "  FAIL  $$f"; \
			echo "  diff expected actual:"; \
			diff -u \
				<(tr -d '\r' < "$$expected") \
				<(tr -d '\r' < "$$actual") || true; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo "Sema: $$passed passed, $$failed failed"; \
	test $$failed -eq 0

# ============================================================================
# All tests
# ============================================================================

test: test-lexer test-parser test-sema

# ============================================================================
# Debug build
# ============================================================================

debug: CFLAGS := -Wall -Wextra -std=c11 -g -fsanitize=address -Isrc
debug: LDFLAGS := -fsanitize=address
debug: all
