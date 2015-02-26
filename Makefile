	
all:
	$(MAKE) -C src 
	$(MAKE) -C utests
	 
clean: 
	$(MAKE) -C src    clean
	$(MAKE) -C utests clean
	
		
 .PHONY : clean all