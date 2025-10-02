PROGRAMS = checksummer checksummer.unsafe
#	$(sort $(patsubst %.c,%,$(wildcard call[0-9]f.c)))

ASMS := $(patsubst %.c,%.s,$(sort $(wildcard asm[0-9][0-9].c)))
CF_ASMS := $(patsubst %.c,%.s,$(sort $(wildcard controlflow[0-9][0-9].c)))
CALL_ASMS := $(patsubst %.c,%.s,$(sort $(wildcard call[0-9][0-9].c)))
CALL_ASM_OBJECTS := $(patsubst %.s,%.o,$(CALL_ASMS))
ASM_PROGRAMS := $(shell grep -l main $(wildcard call[0-9][0-9].s call[0-9][0-9].c) | sed 's/\.[cs]*//' | sort -u)
DEFAULT_ASM_CXXFLAGS = -O1 -fcf-protection=none
DEFAULT_ASM_CXXFLAGS_DEBUG = -g $(DEFAULT_ASM_CXXFLAGS)

-include conf.mk

all: asm $(PROGRAMS)

asm: cleanasm $(ASMS) $(CF_ASMS) $(CALL_ASMS)

ALLPROGRAMS = $(PROGRAMS)

O = 2
include ../common/rules.mk

CXXFLAGS += -fcf-protection=none
UNSAFEFLAGS := -U_FORTIFY_SOURCE -fno-stack-protector -no-pie -fcf-protection=none

checksummer.o: checksummer.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) -U_FORTIFY_SOURCE -no-pie $(CXXFLAGS) $(DEPCFLAGS) -O1 -o $@ -c $<

checksummer.unsafe.o: checksummer.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) $(UNSAFEFLAGS) $(CXXFLAGS) $(DEPCFLAGS) -O1 -o $@ -c $<

%.unsafe.o: %.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) $(UNSAFEFLAGS) $(CXXFLAGS) -O1 $(DEPCFLAGS) -o $@ -c $<

%.unsafe.s: %.cc Makefile
	@ARGS=$$(grep '^//!' $< | sed 's/.*! *//'); \
	  CXXFLAGS="`echo "$(UNSAFEFLAGS) $(CXXFLAGS) -O1" | sed 's/ *-g */ /'`"; \
	  if test -z "$$ARGS"; then ARGS="$(DEFAULT_ASM_CXXFLAGS)"; fi; \
	  $(call xrun,$(CXX) $$CXXFLAGS $$ARGS -o $@ -S $<,COMPILE -S $$ARGS $<) && { $(call cleanasm,$@); }

checksummer: checksummer.o
	@$(call xrun,$(CXX) $(CXXFLAGS) $(O) $(LDFLAGS) -o $@ $^ $(LIBS),LINK $@)

checksummer.unsafe: checksummer.unsafe.o
	@$(call xrun,$(CXX) $(CXXFLAGS) $(UNSAFEFLAGS) $(O) $(LDFLAGS) -o $@ $^ $(LIBS),LINK $@)

%.o: %.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(O) -o $@ -c $<

%.noopt.o: %.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) -O0 -o $@ -c $<

%.opt.o: %.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) -O3 -o $@ -c $<

%.unsafe.o: %.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) $(UNSAFEFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(O) -o $@ -c $<

%.unsafenoopt.o: %.cc $(BUILDSTAMP)
	$(CXX) $(CPPFLAGS) $(UNSAFEFLAGS) $(CXXFLAGS) $(DEPCFLAGS) -O0 -o $@ -c $<


$(ASM_OBJECTS): %.o: %.s $(BUILDSTAMP)
	$(call run,$(CC) -o $@ -c,ASSEMBLE,$<)

%.s: %.c Makefile
	@ARGS=$$(grep '^//!' $< | sed 's/.*! *//'); \
	  CXXFLAGS="`echo "$(CXXFLAGS)" | sed 's/ *-g */ /'`"; \
	  if test -z "$$ARGS"; then ARGS="$(DEFAULT_ASM_CXXFLAGS)"; fi; \
	  $(call xrun,$(CXX) $$CXXFLAGS $$ARGS -o $@ -S $<,COMPILE -S $$ARGS $<) && { $(call cleanasm,$@); }

%.unsafe.s: %.c Makefile
	@ARGS=$$(grep '^//!' $< | sed 's/.*! *//'); \
	  CXXFLAGS="`echo "$(UNSAFEFLAGS) $(CXXFLAGS)" | sed 's/ *-g */ /'`"; \
	  if test -z "$$ARGS"; then ARGS="$(DEFAULT_ASM_CXXFLAGS)"; fi; \
	  $(call xrun,$(CXX) $$CXXFLAGS $$ARGS -o $@ -S $<,COMPILE -S $$ARGS $<) && { $(call cleanasm,$@); }

$(ASM_PROGRAMS): %: %.s
	@ARGS=$$(grep '^//!' $< | sed 's/.*! *//'); \
	  CXXFLAGS="`echo "$(CXXFLAGS)" | sed 's/ *-g */ /;s/ *-std=[^ ]* */ /'`"; \
	  if test -z "$$ARGS"; then ARGS="$(DEFAULT_ASM_CXXFLAGS)"; fi; \
	  $(call xrun,$(CXX) $$CXXFLAGS $$ARGS -o $@ $<,ASSEMBLE $@)

allasmc.txt: $(wildcard asm*.c) always
	for i in asm*.c; do echo $$i; echo ========; cat $$i; echo; echo; done > $@

allasms.txt: $(patsubst %.c,%.s,$(wildcard asm*.c)) always
	for i in asm*.s; do echo $$i; echo ========; cat $$i; echo; echo; done > $@

$(CALL_ASM_OBJECTS): %.o: %.s $(BUILDSTAMP)
	$(call run,$(CC) -o $@ -c,ASSEMBLE,$<)

call04f: call04f.c call04.c
	$(CC) $(CFLAGS) -fomit-frame-pointer -O1 -o $@ $^

#backwards: backwards.c asm06.c
#	$(CC) $(CFLAGS) -fomit-frame-pointer -O1 -o $@ $^

clean:
	rm -f $(ALLPROGRAMS) *.o
	rm -rf $(DEPSDIR) *.dSYM

cleanasm: clean
	rm -f $(patsubst %.c,%.s,$(wildcard asm*.c))
	rm -f $(patsubst %.c,%.s,$(wildcard controlflow*.c))
	rm -f $(patsubst %.c,%.s,$(wildcard call*.c))

.PHONY: all clean cleanasm asm
