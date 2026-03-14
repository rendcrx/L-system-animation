# KOCHSNOWFLAKE | FRACTALPLANT
CLASS = KOCHSNOWFLAKE

CFLAGS = -O2 -D$(CLASS) -g
LIBS = -lm -lSDL3
TARGET = animation

ifeq ($(CLASS), KOCHSNOWFLAKE)
TIMES = 4
else ifeq ($(CLASS), FRACTALPLANT)
TIMES = 5
endif

all: $(TARGET)

$(TARGET): animation.c Makefile
	$(CC) $(CFLAGS) -o $(TARGET) animation.c $(LIBS)

run: $(TARGET)
	@echo "==============$(CLASS)=================="
	@echo "* default: $(TIMES) times"
	@echo "* manual: ./animation <iterate_number>"
	@echo "============================================="
	@./$(TARGET) $(TIMES)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
