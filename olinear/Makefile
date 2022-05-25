CC=g++
CFLAGS=-I.
TYPE = olinear
DEPS = $(TYPE).h primes.h
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
