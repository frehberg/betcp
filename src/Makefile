
CFLAGS=-g

librttcp.a: rttcp.o
	mkdir -p ../lib
	ar  rcs ../lib/librttcp.a rttcp.o


all: librttcp.a 
	
clean:
	rm -rf *.a *.o
	
	
.PHONY : clean all