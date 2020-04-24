CC = gcc
LIBS = -lX11 -lXext -lXfixes
SOURCES=windump.c
OBJS=$(SOURCES:.c=.o)
EXECUTABLE=windump
OPTIMIZE=2
DEBUG=-ggdb3
#DEBUG=
CFLAGS=-O$(OPTIMIZE) $(DEBUG) -std=gnu99 -pedantic -Wall -Wextra
LDFLAGS=$(DEBUG) -Wl,-O$(OPTIMIZE) -Wall -Wextra -Werror

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean all

clean:
	\rm -f $(OBJS) $(EXECUTABLE) *~ core
