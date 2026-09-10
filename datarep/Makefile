CC=clang
PROGRAMS = add addin mexplore.noopt mexplore.opt \
					 mexplore mexplore-order.noopt mexplore-order.opt \
					 mexplore-string mexplore-uninitialized ptr-intro \
					 l04-review mexplore-array mexplore-struct \
					 mexplore-structalign.noopt mexplore-structalign.opt \
					 mexplore-with-dynamic \
					 linked-list signed-int ubexplore.noopt ubexplore.opt \
					 ubexplore2.noopt ubexplore2.opt
ALLPROGRAMS = $(PROGRAMS)

all: $(ALLPROGRAMS)

include ../common/rules.mk

CFLAGS += -Wno-unused-parameter

%.o: %.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) $(O) -o $@ -c $<

%.opt.o: %.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) -O3 $(DEPCFLAGS) -o $@ -c $<

%.noopt.o: %.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) -O0 $(DEPCFLAGS) -o $@ -c $<

add: add.o addf.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(O) -o $@ $^

addin: addin.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(O) -o $@ $^

addf.o: addf.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) $(O) -fomit-frame-pointer -o $@ -c $<

ptr-intro: ptr-intro.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O0 -o $@ $^

l04-review: l04-review.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O0 -o $@ $^

mexplore mexplore-uninitialized mexplore-string mexplore-array mexplore-struct mexplore-with-dynamic linked-list signed-int: %: %.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O0 -o $@ $^

mexplore-structalign.noopt mexplore-order.noopt mexplore.noopt: %.noopt: %.noopt.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O0 -o $@ $^

mexplore-structalign.opt mexplore-order.opt mexplore.opt: %.opt: %.opt.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O3 -o $@ $^

ubexplore.noopt ubexplore2.noopt: %.noopt: %.noopt.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O0 -o $@ $^

ubexplore.opt ubexplore2.opt: %.opt: %.opt.o hexdump.o
	$(CC) $(CFLAGS) $(LDFLAGS) -O3 -o $@ $^

clean:
	rm -rf $(ALLPROGRAMS) *.o $(DEPSDIR)

.PHONY: all clean
