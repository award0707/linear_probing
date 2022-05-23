CC=g++
CFLAGS=-I.
TYPE = linear
DEPS = $(TYPE).h
OBJ = $(TYPE).o bench.o 

all: $(TYPE)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tabletest: $(TYPE).o tabletest.o
	$(CC) -o $@ $^ $(CFLAGS)

$(TYPE): $(TYPE).o bench.o
	$(CC) -o $@ $^ $(CFLAGS)

clean :
	rm -f $(TYPE) $(OBJ)
