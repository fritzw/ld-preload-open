CFLAGS += -std=c99 -Wall

SRCDIR = $(CURDIR)
TESTDIR ?= /tmp/path-mapping
TESTTOOLS = testtool-execl testtool-printenv testtool-utime testtool-ftw testtool-nftw testtool-fts
UNIT_TESTS = test-pathmatching

path-mapping.so: path-mapping.c
	gcc $(CFLAGS) -shared -fPIC path-mapping.c -o path-mapping.so -ldl

all: path-mapping.so testtools unit_tests

clean:
	rm -f *.so
	rm -rf $(TESTDIR)

test: path-mapping.so unit_tests testtools
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

.PHONY: all clean test unit_tests testtools