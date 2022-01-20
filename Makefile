CFLAGS += -std=c99 -Wall

path-mapping.so: path-mapping.c
	gcc $(CFLAGS) -shared -fPIC path-mapping.c -o path-mapping.so -ldl

clean:
	rm *.so

TESTDIR?=/tmp
test: path-mapping.c test-cases.c
	src="$$PWD"; cd $(TESTDIR); gcc $(CFLAGS) "$$src/test-cases.c" "$$src/path-mapping.c" -ldl -o $(TESTDIR)/pathmapping-test
	$(TESTDIR)/pathmapping-test

.PHONY: test clean