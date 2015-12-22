CC ?= gcc
CCFLAGS =-DLOGGER
ARGS=-Wall -Werror -march=native
CCLIBS = -pthread
OBJ = main.o ticketLock.o p2p.o coB.o
PROG = s4

all: $(PROG)

$(PROG) : $(OBJ)
	$(CC) $(ARGS) $(CCFLAGS) -o $(PROG) $(OBJ) $(CCLIBS)

run: all
	./s4

debug: clean
	$(CC) $(ARGS) $(CCFLAGS) -O0 -g -c *.c $(CCLIBS) 
	$(CC) $(ARGS) $(CCFLAGS) -O0 -g -o $(PROG) $(OBJ) $(CCLIBS)

profile:
	$(CC) $(ARGS) $(CCFLAGS) -g -pg -c *.c $(CCLIBS)
	$(CC) $(ARGS) $(CCFLAGS) -g -pg -o $(PROG) $(OBJ) $(CCLIBS)

.c.o:
	$(CC) $(ARGS) $(CCFLAGS) -O3 -c $*.c

clean: 
	-rm *.o
	-rm s4
