CC ?= gcc
CCFLAGS = -DLOGGER
CCLIBS = -pthread
OBJ = main.o p2p.o
PROG = s4

all: $(PROG)

$(PROG) : $(OBJ)
	$(CC) $(CCFLAGS) -o $(PROG) $(OBJ) $(CCLIBS)

debug: 
	$(CC) $(CCFLAGS) -g -c *.c
	$(CC) $(CCFLAGS) -g -o $(PROG) $(OBJ) $(CCLIBS)

.c.o:
	$(CC) $(CCFLAGS) -c $*.c

clean: 
	-rm *.o
	-rm s4
