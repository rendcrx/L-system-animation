CFLAGS = -Wall -Werror -std=c99 -pedantic -O2
LIBS   = -lm -lSDL3
TARGET = animation

all: $(TARGET)

$(TARGET): animation.c Makefile
	$(CC) $(CFLAGS) -o $(TARGET) animation.c $(LIBS)

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
