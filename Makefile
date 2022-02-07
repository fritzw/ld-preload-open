override CFLAGS += -std=c99 -Wall

SRCDIR = $(CURDIR)
TESTDIR ?= /tmp/path-mapping
TESTTOOLS = $(notdir $(basename $(wildcard $(SRCDIR)/test/testtool-*.c)))
UNIT_TESTS = test-pathmatching

path-mapping.so: path-mapping.c
	gcc $(CFLAGS) -shared -fPIC path-mapping.c -o $@ -ldl

path-mapping-debug.so: path-mapping.c
	gcc $(CFLAGS) -DDEBUG -shared -fPIC path-mapping.c -o $@ -ldl

path-mapping-quiet.so: path-mapping.c
	gcc $(CFLAGS) -DQUIET -shared -fPIC path-mapping.c -o $@ -ldl

all: path-mapping.so path-mapping-debug.so path-mapping-quiet.so

clean:
	rm -f *.so
	rm -rf $(TESTDIR)

test: all unit_tests testtools
	for f in $(UNIT_TESTS); do $(TESTDIR)/$$f; done
	TESTDIR="$(TESTDIR)" test/integration-tests.sh

unit_tests: $(addprefix $(TESTDIR)/, $(UNIT_TESTS))

testtools: $(addprefix $(TESTDIR)/, $(TESTTOOLS))

$(TESTDIR)/test-%: $(SRCDIR)/test/test-%.c $(SRCDIR)/path-mapping.c
	mkdir -p $(TESTDIR)
	cd $(TESTDIR); gcc $(CFLAGS) $< "$(SRCDIR)/path-mapping.c" -ldl -o $@

$(TESTDIR)/testtool-%: $(SRCDIR)/test/testtool-%.c
	mkdir -p $(TESTDIR)
	cd $(TESTDIR); gcc $(CFLAGS) $^ -o $@

.PHONY: all libs clean test unit_tests testtools
