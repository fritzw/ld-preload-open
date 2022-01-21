CFLAGS += -std=c99 -Wall

all: path-mapping.so

path-mapping.so: path-mapping.c
	gcc $(CFLAGS) -shared -fPIC path-mapping.c -o path-mapping.so -ldl

clean:
	rm *.so

TESTDIR?=/tmp/path-mapping
test: path-mapping.c test/unit-tests.c
	src="$$PWD"; mkdir -p $(TESTDIR); cd $(TESTDIR); gcc $(CFLAGS) "$$src/test/unit-tests.c" "$$src/path-mapping.c" -ldl -o $(TESTDIR)/unit-tests
	$(TESTDIR)/unit-tests
	test/integration-tests.sh

.PHONY: all test clean