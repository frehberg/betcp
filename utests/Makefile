
all: 
	$(MAKE) -C non_reading all
	$(MAKE) -C slow_reader all
	$(MAKE) -C c_build     all
	
clean:
	$(MAKE) -C non_reading clean
	$(MAKE) -C slow_reader clean
	$(MAKE) -C c_build     clean
	
utest:
	$(MAKE) -C non_reading utest
	$(MAKE) -C slow_reader utest
	$(MAKE) -C c_build     utest
	
 .PHONY : clean all