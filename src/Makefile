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
OBJDIR = ../obj
BINDIR = ../bin
SRC = $(tabletypes:%=tables/%.cc) 
OBJ = $(tabletypes:%=$(OBJDIR)/%.o) $(OBJDIR)/primes.o $(OBJDIR)/util.o

all: tests

$(OBJDIR)/%.o: hashtables/%.cc $(TABLEDEPS)
	mkdir -p $(OBJDIR)
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

$(OBJDIR)/%.o: %.cc $(TABLEDEPS) $(TESTERDEPS) 
	mkdir -p $(OBJDIR)
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

$(OBJDIR)/%.o: tools/%.cc $(wildcard tools/*)
	mkdir -p $(OBJDIR)
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

$(benches): %: $(OBJDIR)/%.o $(OBJ)
	mkdir -p $(BINDIR)
	$(CC) -o $(BINDIR)/$@ $^ $(CFLAGS) $(INC)

tests: $(benches)

test:
	@echo $(SRC)
	@echo $(OBJ)

clean:
	-rm $(OBJDIR)/*.o -f $(addprefix $(BINDIR)/, $(benches))

