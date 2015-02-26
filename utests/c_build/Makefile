CFLAGS=-g
APP=tc_c_build

all: $(APP).c
	mkdir -p bld
	gcc $(CFLAGS) $(APP).c -I../../src -L../../lib -lrttcp -o bld/$(APP)

clean:
	rm -rf bld *.o

utest:
	$(APP)
	
 .PHONY : clean all
