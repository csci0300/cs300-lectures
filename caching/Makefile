PROGRAMS = arrayaccess diskio-slow diskio-fast read read-caching
all: $(PROGRAMS)

ALLPROGRAMS = $(PROGRAMS)

include ../common/rules.mk

%.o: %.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(O) -o $@ -c $<

cpp%: cpp%.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -O0 -o $@ $^

diskio-%: diskio-%.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

arrayaccess: arrayaccess.o qslib.o allowexec.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

arrayinsert0: arrayinsert0.o qslib.o allowexec.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

arrayinsert1: arrayinsert1.o qslib.o allowexec.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

arrayinsert2: arrayinsert1.o qslib.o allowexec.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

read: read.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

read-caching: read-caching.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(O) -o $@ $^

clean:
	rm -rf $(ALLPROGRAMS) *.o $(DEPSDIR)

.PHONY: all clean
