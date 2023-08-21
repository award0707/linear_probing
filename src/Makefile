CC=g++ -std=c++2a
#CFLAGS=-ggdb -O0
CFLAGS=-O2 -Wall
INC = -I. -Itesters -Ihashtables -Itools

tabletypes = graveyard_aos ordered_aos linear_aos graveyard_soa \
	     ordered_soa linear_soa
testers = querytester
benches = tabletest querystats queuestats xtester rebuildstats \
	  floatstats loadbench 

DEPS = $(wildcard tools/primes.*) $(wildcard tables/*.h) \
       $(wildcard testers/*.hpp)
SRC = $(tabletypes:%=tables/%.cc) 
OBJ = $(tabletypes:%=obj/%.o) obj/primes.o obj/util.o

all: tests

obj/%.o: hashtables/%.cc $(DEPS)
	mkdir -p obj
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

obj/%.o: %.cc $(DEPS) 
	mkdir -p obj
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

obj/%.o: tools/%.cc $(DEPS)
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
	rm obj/*.o $(benches)

