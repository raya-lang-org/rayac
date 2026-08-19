# Compiler and flags
CC       := C:\ProgramData\mingw64\mingw64\bin\gcc.exe
CFLAGS = -Wall -Wextra -Werror -std=c11 -O2 -I.
LDFLAGS =

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET = $(BINDIR)/raya

.PHONY: all clean test dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

test: all
	@echo "Running lexer tests..."
	@for f in tests/lexer/*.raya; do \
		echo "  $$f"; \
		$(TARGET) --dump-tokens "$$f" > /dev/null 2>&1 && echo "    PASS" || echo "    FAIL"; \
	done

debug: CFLAGS = -Wall -Wextra -std=c11 -g -fsanitize=address -I.
debug: LDFLAGS = -fsanitize=address
debug: all
