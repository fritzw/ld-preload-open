CFLAGS += -std=c99 -Wall

log_file_accesses.so:
	gcc $(CFLAGS) -shared -fPIC path-mapping.c -o path-mapping.so -ldl

clean:
	rm *.so
