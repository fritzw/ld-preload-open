log_file_accesses.so:
	gcc -shared -fPIC log_file_access.c -o log_file_access.so -ldl

clean:
	rm *.so
