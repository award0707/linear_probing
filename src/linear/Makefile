CC=g++ -std=c++2a
#CFLAGS=-ggdb
CFLAGS=-O2 -Wall
INC = -I. -I../tools
OBJDIR = obj

TABLETYPE = linear

DEPS = $(wildcard ../tools/primes.*) $(wildcard *.h)
SRC = $(TABLETYPE).c
OBJ = $(SRC:%.c=$(OBJDIR)/%.o) $(OBJDIR)/primes.o

all: tabletest loadbench

bench: runbench loadbench

$(OBJDIR)/%.o: %.c $(DEPS) 
	mkdir -p $(OBJDIR)
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

$(OBJDIR)/primes.o: ../tools/primes.c
	mkdir -p $(OBJDIR)
	@echo $@
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

tabletest: $(OBJDIR)/tabletest.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(INC)

bench: $(OBJDIR)/bench.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(INC)

loadbench: $(OBJDIR)/loadbench.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(INC)

test:
	@echo $(DEPS)
	@echo $(SRC)
	@echo $(OBJ)

clean :
	rm $(OBJDIR)/* bench tabletest loadbench

