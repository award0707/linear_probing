CC=g++ -std=c++2a
#CFLAGS=-ggdb -O0 -Wall
CFLAGS=-O2 -Wall
INC = -I. -Itesters -Ihashtables -Itools

tabletypes = graveyard_aos ordered_aos linear_aos graveyard_soa \
	     ordered_soa linear_soa
testers = amorttester querytester rebuildtester loadtester floattester \
	  one_rb_querytester
benches = tabletest querystats queuestats xtester rebuildstats \
	  floatstats loadstats amortstats

TABLEDEPS = $(wildcard tools/*) $(wildcard hashtables/*.h)
TESTERDEPS = $(wildcard tools/*) $(wildcard testers/*.hpp)
SRC = $(tabletypes:%=tables/%.cc) 
OBJ = $(tabletypes:%=obj/%.o) obj/primes.o obj/util.o

all: tests

obj/%.o: hashtables/%.cc $(TABLEDEPS)
	mkdir -p obj
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

obj/%.o: %.cc $(TABLEDEPS) $(TESTERDEPS) 
	mkdir -p obj
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

obj/%.o: tools/%.cc $(wildcard tools/*)
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
	-rm obj/*.o -f $(benches)

