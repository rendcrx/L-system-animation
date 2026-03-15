# CFLAGS = -Wall -Werror -std=c99 -pedantic -O2
CFLAGS = -std=c99 -pedantic -O2
LIBS = -lm -lSDL3
TARGET = animation

all: $(TARGET)

$(TARGET): animation.c Makefile
	$(CC) $(CFLAGS) -o $(TARGET) animation.c $(LIBS)

run: $(TARGET)
	@echo "========================================"
	@echo "* PRESS   1: koch snowflake animation"
	@echo "* PRESS   2: fractl plant animation"
	@echo "* PRESS   3: probabilistic animation"
	@echo "* PRESS   4: sierpinski triangle animation"
	@echo "* PRESS   b: starfield animation"
	@echo "* PRESS   c: clean screen"
	@echo "* PRESS ESC: quit window"
	@echo "========================================"
	@./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
