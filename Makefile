CC = gcc

# Compile flags (Include headers, warnings, sanitizers)
CFLAGS = -std=gnu99 -Wall -fsanitize=address,undefined

# Linker flags (Sanitizers need to be linked too)
LDFLAGS = -fsanitize=address,undefined

# Library flags (Math and Pthread)
LDLIBS = -lpthread -lm

SRC = dicegame.c
TARGET = dicegame

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)