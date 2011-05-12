CC = gcc
LIBS = -lX11 -lXext -lXtst -lXfixes
CFLAGS = -O2
OBJ = windump.o

windump: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core windump 

