###### generic rules #######

#release
#==============================================
%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	@echo CC $@ 
	@$(CC) $(CFLAGS) -shared -fPIC -c -o $@ $< 

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

%.o: %.m
	$(CC) $(CFLAGS) -c -o $@ $<

%-rc.o: %.rc
	$(WINDRES) -I. $< -o $@
#==============================================

#debug
#==============================================
%.debug.o: %.S
	$(CC) $(CFLAGS) -g -c -o $@ $<

%.debug.o: %.c
	@echo CC $@ 
	@$(CC) $(CFLAGS) -g -c -o $@ $< 

%.debug.o: %.cpp
	$(CXX) $(CXXFLAGS) -g -c -o $@ $<

%.debug.o: %.m
	$(CC) $(CFLAGS) -g -c -o $@ $<
#==============================================

