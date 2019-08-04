
CC=g++
CFLAGS=-I.
DEPS = netsnoop.h
OBJ = netsnoop.o

netsnoop: $(OBJ)
	$(CC) -g -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

