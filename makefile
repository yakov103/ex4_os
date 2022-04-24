CC=g++
CFlags=-g -Wall
BINS=server
OBJS=server.o myqueue.o mystack.o

all: $(BINS)

server: $(OBJS)
	$(CC) $(CFlags) -o $@  $^ -lpthread

%: %.cpp
	$(CC) $(CFlags) -c -o $@  $^ -lpthread

clean:
	rm -f *.dSYM $(BINS) *.o