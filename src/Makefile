CC=g++ -std=c++2a
CFLAGS=-I.
TYPES = linear quadratic ordered graveyard
DEPS = $(patsubst %,%.h,$(TYPES)) hashtable.h primes.h bench.h
TESTOBJ = $(patsubst %,%.o,$(TYPES)) tabletest.o hashtable.o primes.o
OBJ = $(patsubst %,%.o,$(TYPES)) bench.o hashtable.o primes.o

all: bench tabletest

%.o: %.c $(DEPS) 
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS)

tabletest: hashtable.o tabletest.o $(TESTOBJ)
	$(CC) -o $@ $^ $(CFLAGS)

bench: hashtable.o bench.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

test:
	@echo $(OBJ)

clean :
	rm -f $(OBJ) bench tabletest

