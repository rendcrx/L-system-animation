CFLAGS = -Wall -Werror -std=c99 -pedantic -O2
LIBS   = -lm -lSDL3
TARGET = animation

all: $(TARGET)

$(TARGET): animation.c Makefile
	$(CC) $(CFLAGS) -o $(TARGET) animation.c $(LIBS)

run: $(TARGET)
	@echo "========================================"
	@echo "* PRESS   1: Koch snowflake animation"
	@echo "* PRESS   2: Fractl plant animation"
	@echo "* PRESS   3: Probabilistic animation"
	@echo "* PRESS   4: Sierpinski triangle animation"
	@echo "* PRESS   5: Dragon curve animation"
	@echo "* PRESS   b: Starfield animation"
	@echo "* PRESS   c: Clean screen"
	@echo "* PRESS ESC: Quit window"
	@echo "========================================"
	@./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
