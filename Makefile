CC ?= gcc
CCFLAGS = -DLOGGER
ARGS=-Wall -Werror
CCLIBS = -pthread
OBJ = main.o p2p.o coB.o
PROG = s4

all: $(PROG)

$(PROG) : $(OBJ)
	$(CC) $(ARGS) $(CCFLAGS) -o $(PROG) $(OBJ) $(CCLIBS)

debug: 
	$(CC) $(ARGS) $(CCFLAGS) -g -c *.c
	$(CC) $(ARGS) $(CCFLAGS) -g -o $(PROG) $(OBJ) $(CCLIBS)

profile:
	$(CC) $(ARGS) $(CCFLAGS) -g -pg -c *.c
	$(CC) $(ARGS) $(CCFLAGS) -g -pg -o $(PROG) $(OBJ) $(CCLIBS)

.c.o:
	$(CC) $(ARGS) $(CCFLAGS) -c $*.c

clean: 
	-rm *.o
	-rm s4
