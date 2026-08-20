# Compiler and flags
CC       := C:\ProgramData\mingw64\mingw64\bin\gcc.exe
CFLAGS   := -Wall -Wextra -Werror -std=c11 -O2 -Isrc
LDFLAGS  :=

SRCDIR   := src
OBJDIR   := obj
SRCS     := src/main.c src/lexer.c src/parser.c src/ast.c src/arena.c src/diag.c \
            src/type.c src/symbol.c src/sema.c

BINDIR   := bin
TESTDIR  := tests

SOURCES  := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET   := $(BINDIR)/raya

.PHONY: all clean test test-lexer test-parser dirs

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
# Fast lexer tests — no awk, just raw token kinds
# ==========================================================================
test-lexer: all
	@echo "Running lexer tests..."
	@set -e; \
	passed=0; failed=0; \
	for f in $(TESTDIR)/lexer/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="$(TESTDIR)/lexer/$$name.expected"; \
		if [ ! -f "$$expected" ]; then \
			echo "  SKIP  $$f (no .expected)"; \
			continue; \
		fi; \
		actual="$(TESTDIR)/lexer/$$name.actual"; \
		$(TARGET) --test-lexer "$$f" > "$$actual" 2>/dev/null; \
		if cmp -s "$$expected" "$$actual"; then \
			echo "  PASS  $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo "  FAIL  $$f"; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo "Lexer: $$passed passed, $$failed failed"

# ============================================================================
# Fast parser tests — AST kind tree output
# ==========================================================================
test-parser: all
	@echo "Running parser tests..."
	@set -e; \
	passed=0; failed=0; \
	for f in $(TESTDIR)/parser/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="$(TESTDIR)/parser/$$name.expected"; \
		if [ ! -f "$$expected" ]; then \
			echo "  SKIP  $$f (no .expected)"; \
			continue; \
		fi; \
		actual="$(TESTDIR)/parser/$$name.actual"; \
		$(TARGET) --test-parser "$$f" > "$$actual" 2>/dev/null; \
		if cmp -s "$$expected" "$$actual"; then \
			echo "  PASS  $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo "  FAIL  $$f"; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo "Parser: $$passed passed, $$failed failed"
	

test-sema: all
	@echo "Running sema tests..."
	@passed=0; failed=0; \
	for f in $(TESTDIR)/sema/*.raya; do \
		name=$$(basename "$$f" .raya); \
		expected="$(TESTDIR)/sema/$$name.expected"; \
		if [ ! -f "$$expected" ]; then \
			echo " SKIP $$f (no .expected)"; \
			continue; \
		fi; \
		actual="$(TESTDIR)/sema/$$name.actual"; \
		$(TARGET) --test-sema "$$f" > "$$actual" 2>/dev/null; \
		if cmp -s "$$expected" "$$actual"; then \
			echo " PASS $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo " FAIL $$f"; \
			echo "  diff expected actual:"; \
			diff "$$expected" "$$actual" | sed 's/^/    /' || true; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo "Sema: $$passed passed, $$failed failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi

test: test-lexer test-parser test-sema

debug: CFLAGS := -Wall -Wextra -std=c11 -g -fsanitize=address -Isrc
debug: LDFLAGS := -fsanitize=address
debug: all
