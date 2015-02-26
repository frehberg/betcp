
CFLAGS=-g
APP=tc_fast_reader

all: $(APP).cpp
	mkdir -p bld
	g++ $(CFLAGS) $(APP).cpp -I../../src -lboost_thread -lboost_system -lcppunit -L../../lib -lrttcp -o bld/$(APP)

clean:
	rm -rf bld *.o
	
utest:
	$(APP)
	
 .PHONY : clean all
