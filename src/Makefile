CC=g++ -std=c++2a
#CFLAGS=-ggdb -O0
CFLAGS=-O2 -Wall
INC = -I. -Itesters -I tables -Itools

tabletypes = graveyard ordered linear
testers = querytester
benches = tabletest querystats queuestats xtester rebuildstats \
	  floatstats loadbench 

DEPS = $(wildcard ../tools/primes.*) $(wildcard *.h) $(testers:=.hpp)
SRC = $(tabletypes:=.cc) 
OBJ = $(tabletypes:%=obj/%.o) obj/primes.o

all: tests

obj/%.o: %.cc $(DEPS) 
	mkdir -p obj
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

obj/primes.o: ../tools/primes.c
	mkdir -p obj
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

$(benches): %: obj/%.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(INC)

tests: $(benches)

test:
	@echo $(SRC)
	@echo $(OBJ)

clean :
	rm obj/* $(benches)

