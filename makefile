CC = gcc
CFLAGS = -std=gnu99 -Wall -g -pedantic -DISR3_VERBOSE
LDFLAGS =

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

OUTPUT = isr-prog3

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	@echo LD $(OBJECTS)
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $(OUTPUT)

%.o: %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS)

cleanbin: clean
	rm -f $(OUTPUT)
